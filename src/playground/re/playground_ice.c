/**
 * @file ice.c ICE Testcode
 *
 * Copyright (C) 2010 Creytiv.com
 * Copyright (C) 2020 TLM Partners Inc.
 */

#include "rawr/Error.h"

#include "mn/log.h"

#include <re.h>
#include <string.h>

#define DEBUG_MODULE "test_ice"
#define DEBUG_LEVEL 5
#include <re_dbg.h>


#define ESKIPPED (-1000)

#define TEST_EQUALS(expected, actual)                          \
    if ((expected) != (actual)) {                              \
        mn_log_error("TEST_EQUALS: %s:%u: %s():"               \
                      " expected=%d(0x%x), actual=%d(0x%x)\n", \
                      __FILE__,                                \
                      __LINE__,                                \
                      __func__,                                \
                      (expected),                              \
                      (expected),                              \
                      (actual),                                \
                      (actual));                               \
        err = EINVAL;                                          \
        goto out;                                              \
    }

#define TEST_NOT_EQUALS(expected, actual)            \
    if ((expected) == (actual)) {                    \
        mn_log_error("TEST_NOT_EQUALS: %s:%u:"      \
                      " expected=%d != actual=%d\n", \
                      __FILE__,                      \
                      __LINE__,                      \
                      (expected),                    \
                      (actual));                     \
        err = EINVAL;                                \
        goto out;                                    \
    }

#define TEST_MEMCMP(expected, expn, actual, actn)                \
    if (expn != actn ||                                          \
        0 != memcmp((expected), (actual), (expn))) {             \
        mn_log_error("TEST_MEMCMP: %s:%u:"                      \
                      " %s(): failed\n",                         \
                      __FILE__,                                  \
                      __LINE__,                                  \
                      __func__);                                 \
        test_hexdump_dual(stderr, expected, expn, actual, actn); \
        err = EINVAL;                                            \
        goto out;                                                \
    }

#define TEST_STRCMP(expected, expn, actual, actn)                 \
    if (expn != actn ||                                           \
        0 != memcmp((expected), (actual), (expn))) {              \
        mn_log_error("TEST_STRCMP: %s:%u:"                        \
                      " failed\n",                                \
                      __FILE__,                                   \
                      __LINE__);                                  \
        (void)re_fprintf(stderr, "expected string: (%zu bytes)\n" \
                                 "\"%b\"\n",                      \
                         (size_t)(expn),                          \
                         (expected),                              \
                         (size_t)(expn));                         \
        (void)re_fprintf(stderr, "actual string: (%zu bytes)\n"   \
                                 "\"%b\"\n",                      \
                         (size_t)(actn),                          \
                         (actual),                                \
                         (size_t)(actn));                         \
        err = EINVAL;                                             \
        goto out;                                                 \
    }

#define TEST_ASSERT(actual)                 \
    if (!(actual)) {                        \
        mn_log_error("TEST_ASSERT: %s:%u:"  \
                      " actual=%d\n",       \
                      __FILE__,             \
                      __LINE__,             \
                      (actual));            \
        err = EINVAL;                       \
        goto out;                           \
    }

#define TEST_ERR(err)                    \
    if ((err)) {                         \
        mn_log_error("TEST_ERR: %s:%u:"  \
                      " (%m)\n",         \
                      __FILE__,          \
                      __LINE__,          \
                      (err));            \
        goto out;                        \
    }

#define TEST_SACMP(expect, actual, flags)          \
    if (!sa_cmp((expect), (actual), (flags))) {    \
                                                   \
        mn_log_error("TEST_SACMP: %s:%u:"          \
                      " %s(): failed\n",           \
                      __FILE__,                    \
                      __LINE__,                    \
                      __func__);                   \
        DEBUG_WARNING("expected: %J\n", (expect)); \
        DEBUG_WARNING("actual:   %J\n", (actual)); \
        err = EADDRNOTAVAIL;                       \
        goto out;                                  \
    }

/*
 * Protocol testcode for 2 ICE agents. We setup 2 ICE agents A and B,
 * with only a basic host candidate. Gathering is done using a small
 * STUN Server running on localhost, SDP is exchanged with Offer/Answer.
 * Finally the connectivity checks are run and the result is verified.
 *
 *
 *                    .-------------.
 *                    | STUN Server |
 *                    '-------------'
 *              STUN /               \ STUN
 *                  /                 \
 *                 /                   \
 *  .-------------.                     .-------------.
 *  | ICE-Agent A |---------------------| ICE-Agent B |
 *  '-------------'     Connectivity    '-------------'
 *                         checks
 *
 */

struct stunserver {
    struct udp_sock *us;
    struct tcp_sock *ts;
    struct tcp_conn *tc;
    struct mbuf *mb;
    struct sa laddr;
    struct sa laddr_tcp;
    struct sa paddr;
    uint32_t nrecv;
    int err;
};

int stunserver_alloc(struct stunserver **stunp);
const struct sa *stunserver_addr(const struct stunserver *stun, int proto);

struct turnserver {
    struct udp_sock *us;
    struct sa laddr;
    struct tcp_sock *ts;
    struct sa laddr_tcp;
    struct tcp_conn *tc;
    struct sa paddr;
    struct mbuf *mb;
    struct udp_sock *us_relay;
    struct sa cli;
    struct sa relay;

    struct channel {
        uint16_t nr;
        struct sa peer;
    } chanv[4];
    size_t chanc;

    struct sa permv[4];
    size_t permc;

    size_t n_allocate;
    size_t n_createperm;
    size_t n_chanbind;
    size_t n_send;
    size_t n_raw;
    size_t n_recv;
};

int turnserver_alloc(struct turnserver **turnp);

struct attrs {
    struct attr {
        char name[16];
        char value[128];
    } attrv[16];
    unsigned attrc;
};

struct agent {
    struct icem *icem;
    struct udp_sock *us;
    struct sa laddr;
    struct attrs attr_s;
    struct attrs attr_m;
    struct ice_test *it; /* parent */
    struct stunserver *stun;
    struct turnserver *turn;
    enum ice_mode mode;
    char name[16];
    uint8_t compid;
    bool offerer;
    bool use_turn;
    size_t n_cand;

    char lufrag[8];
    char lpwd[32];

    /* results: */
    bool gathering_ok;
    bool conncheck_ok;

    struct stun_ctrans *ct_gath;
};

struct ice_test {
    struct agent *a;
    struct agent *b;
    struct tmr tmr;
    int err;
};

static void icetest_check_gatherings(struct ice_test *it);
static void icetest_check_connchecks(struct ice_test *it);

/*
 * Test tools
 */

static void complete_test(struct ice_test *it, int err)
{
    it->err = err;

#if 0
	re_printf("\n\x1b[32m%H\x1b[;m\n", icem_debug, it->a->icem);
	re_printf("\n\x1b[36m%H\x1b[;m\n", icem_debug, it->b->icem);
#endif

    re_cancel();
}

static bool find_debug_string(struct icem *icem, const char *str)
{
    char buf[1024];

    if (re_snprintf(buf, sizeof(buf), "%H", icem_debug, icem) < 0)
        return false;

    return 0 == re_regex(buf, strlen(buf), str);
}

static int attr_add(struct attrs *attrs, const char *name, const char *value, ...)
{
    struct attr *attr = &attrs->attrv[attrs->attrc];
    va_list ap;
    int r, err = 0;

    TEST_ASSERT(attrs->attrc <= ARRAY_SIZE(attrs->attrv));

    TEST_ASSERT(strlen(name) < sizeof(attr->name));
    str_ncpy(attr->name, name, sizeof(attr->name));

    if (value) {
        va_start(ap, value);
        r = re_vsnprintf(attr->value, sizeof(attr->value), value, ap);
        va_end(ap);
        TEST_ASSERT(r > 0);
    }

    attrs->attrc++;

out:
    return err;
}

static const char *attr_find(const struct attrs *attrs, const char *name)
{
    unsigned i;

    if (!attrs || !name)
        return NULL;

    for (i = 0; i < attrs->attrc; i++) {
        const struct attr *attr = &attrs->attrv[i];

        if (0 == str_casecmp(attr->name, name))
            return attr->value;
    }

    return NULL;
}

/*
 * ICE Agent
 */

static void agent_destructor(void *arg)
{
    struct agent *agent = arg;

    mem_deref(agent->icem);
    mem_deref(agent->us);
    mem_deref(agent->stun);
    mem_deref(agent->turn);
}

static struct agent *agent_other(struct agent *agent)
{
    if (agent->it->a == agent)
        return agent->it->b;
    else
        return agent->it->a;
}

static int agent_encode_sdp(struct agent *ag)
{
    struct le *le;
    int err = 0;

    for (le = icem_lcandl(ag->icem)->head; le; le = le->next) {

        struct cand *cand = le->data;

        err = attr_add(&ag->attr_m, "candidate", "%H", ice_cand_encode, cand);
        if (err)
            break;
    }

    err |= attr_add(&ag->attr_m, "ice-ufrag", ag->lufrag);
    err |= attr_add(&ag->attr_m, "ice-pwd", ag->lpwd);

    return err;
}

static int agent_verify_outgoing_sdp(const struct agent *agent)
{
    const char *cand, *ufrag, *pwd;
    char buf[1024];
    int err = 0;

    if (re_snprintf(buf, sizeof(buf), "7f000001 %u UDP 2113929465 192.168.21.39 %u typ host", agent->compid, sa_port(&agent->laddr)) < 0) {
        return ENOMEM;
    }
    cand = attr_find(&agent->attr_m, "candidate");
    //TEST_STRCMP(buf, str_len(buf), cand, str_len(cand));

    ufrag = attr_find(&agent->attr_m, "ice-ufrag");
    pwd = attr_find(&agent->attr_m, "ice-pwd");
    TEST_STRCMP(agent->lufrag, str_len(agent->lufrag), ufrag, str_len(ufrag));
    TEST_STRCMP(agent->lpwd, str_len(agent->lpwd), pwd, str_len(pwd));

    if (agent->mode == ICE_MODE_FULL) {
        TEST_ASSERT(NULL == attr_find(&agent->attr_s, "ice-lite"));
    }

out:
    return err;
}

static int agent_decode_sdp(struct agent *agent, struct agent *other)
{
    unsigned i;
    int err = 0;

    for (i = 0; i < other->attr_s.attrc; i++) {
        struct attr *attr = &other->attr_s.attrv[i];
        err = ice_sdp_decode(agent->icem, attr->name, attr->value);
        if (err)
            return err;
    }

    for (i = 0; i < other->attr_m.attrc; i++) {
        struct attr *attr = &other->attr_m.attrv[i];
        err = icem_sdp_decode(agent->icem, attr->name, attr->value);
        if (err)
            return err;
    }

    return err;
}

static int send_sdp(struct agent *agent)
{
    int err;

    /* verify ICE states */
    TEST_ASSERT(!icem_mismatch(agent->icem));

    /* after gathering is complete we expect:
	 *   1 local candidate
	 *   0 remote candidates
	 *   checklist and validlist is empty
	 */
    //TEST_EQUALS(agent->n_cand, list_count(icem_lcandl(agent->icem)));
    TEST_EQUALS(0, list_count(icem_rcandl(agent->icem)));
    TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
    TEST_EQUALS(0, list_count(icem_validl(agent->icem)));

    //if (agent->use_turn) {
    //    /* verify that default candidate is the relayed address */
    //    TEST_SACMP(&agent->turn->relay, icem_cand_default(agent->icem, agent->compid), SA_ALL);
    //} else {
    //    /* verify that default candidate is our local address */
    //    TEST_SACMP(&agent->laddr, icem_cand_default(agent->icem, agent->compid), SA_ALL);
    //}

    /* we should not have selected candidate-pairs yet */
    TEST_ASSERT(!icem_selected_laddr(agent->icem, agent->compid));

    err = agent_encode_sdp(agent);
    if (err)
        return err;

    err = agent_verify_outgoing_sdp(agent);
    if (err)
        return err;

out:
    return err;
}

static void agent_gather_handler(int err, uint16_t scode, const char *reason, void *arg)
{
    struct agent *agent = arg;

    if (err)
        goto out;
    if (scode) {
        DEBUG_WARNING("gathering failed: %u %s\n", scode, reason);
        complete_test(agent->it, EPROTO);
        return;
    }

    /* Eliminate redundant local candidates */
    icem_cand_redund_elim(agent->icem);

    err = icem_comps_set_default_cand(agent->icem);
    if (err) {
        DEBUG_WARNING("ice: set default cands failed (%m)\n", err);
        goto out;
    }

    agent->gathering_ok = true;

    err = send_sdp(agent);
    if (err)
        goto out;

    icetest_check_gatherings(agent->it);

    return;

out:
    complete_test(agent->it, err);
}

static void stun_resp_handler(int err, uint16_t scode, const char *reason, const struct stun_msg *msg, void *arg)
{
    struct agent *ag = arg;
    struct stun_attr *attr;
    struct ice_cand *lcand;

    if (err || scode > 0) {
        DEBUG_WARNING("STUN Request failed: %m\n", err);
        goto out;
    }

    /* base candidate */
    lcand = icem_cand_find(icem_lcandl(ag->icem), ag->compid, NULL);
    if (!lcand)
        goto out;

    attr = stun_msg_attr(msg, STUN_ATTR_XOR_MAPPED_ADDR);
    if (!attr)
        attr = stun_msg_attr(msg, STUN_ATTR_MAPPED_ADDR);
    if (!attr) {
        DEBUG_WARNING("no Mapped Address in Response\n");
        err = EPROTO;
        goto out;
    }

    err = icem_lcand_add(ag->icem, icem_lcand_base(lcand), ICE_CAND_TYPE_SRFLX, &attr->v.sa);

out:
    agent_gather_handler(err, scode, reason, ag);
}

static int icem_gather_srflx(struct agent *ag, const struct sa *srv)
{
    int err;

    err = stun_request(&ag->ct_gath, icem_stun(ag->icem), IPPROTO_UDP, ag->us, srv, 0, STUN_METHOD_BINDING, NULL, false, 0, stun_resp_handler, ag, 1, STUN_ATTR_SOFTWARE, stun_software);
    if (err)
        return err;

    return 0;
}

static void agent_connchk_handler(int err, bool update, void *arg)
{
    struct agent *agent = arg;
    struct agent *other = agent_other(agent);
    const struct sa *laddr, *raddr;

    if (err) {
        if (err != ENOMEM) {
            DEBUG_WARNING("%s: connectivity checks failed: %m\n", agent->name, err);
        }

        complete_test(agent->it, err);
        return;
    }

    if (agent->offerer ^ update) {
        DEBUG_WARNING("error in update flag\n");
        complete_test(agent->it, EPROTO);
        return;
    }

    agent->conncheck_ok = true;

    /* verify ICE states */
    TEST_ASSERT(!icem_mismatch(agent->icem));

    /* after connectivity checks are complete we expect:
	 *   1 local candidate
	 *   1 remote candidates
	 */
    TEST_EQUALS(agent->n_cand, list_count(icem_lcandl(agent->icem)));
    TEST_EQUALS(other->n_cand, list_count(icem_rcandl(agent->icem)));
    TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
    TEST_EQUALS(agent->n_cand * other->n_cand, list_count(icem_validl(agent->icem)));

    laddr = icem_selected_laddr(agent->icem, agent->compid);
    raddr = &agent_other(agent)->laddr;

    if (!sa_cmp(&agent->laddr, laddr, SA_ALL)) {
        DEBUG_WARNING("unexpected selected address: %J\n", laddr);
        complete_test(agent->it, EPROTO);
        return;
    }

    if (!icem_verify_support(agent->icem, agent->compid, raddr)) {
        complete_test(agent->it, EPROTO);
        return;
    }

#if 0
	(void)re_printf("Agent %s -- Selected address: local=%J  remote=%J\n",
			agent->name, laddr, raddr);
#endif

    icetest_check_connchecks(agent->it);

out:
    if (err)
        complete_test(agent->it, err);
}

static int agent_alloc(struct agent **agentp, struct ice_test *it, enum ice_mode mode, bool use_turn, const char *name, uint8_t compid, bool offerer)
{
    struct agent *agent;
    enum ice_role lrole;
    uint64_t tiebrk;
    int err;

    agent = mem_zalloc(sizeof(*agent), agent_destructor);
    if (!agent)
        return ENOMEM;

    re_snprintf(agent->lufrag, sizeof(agent->lufrag), "ufrag-%s", name);
    re_snprintf(agent->lpwd, sizeof(agent->lpwd), "password-0123456789abcdef-%s", name);

    agent->use_turn = use_turn;
    agent->it = it;
    str_ncpy(agent->name, name, sizeof(agent->name));
    agent->compid = compid;
    agent->offerer = offerer;
    agent->mode = mode;

    if (mode == ICE_MODE_FULL) {

        if (agent->use_turn) {
            //err = turnserver_alloc(&agent->turn);
            //if (err)
                goto out;
        } else {
            err = stunserver_alloc(&agent->stun);
            if (err)
                goto out;
        }
    }

    if (offerer) {
        sa_set_str(&agent->laddr, "192.168.21.39", 0);
    } else {
        sa_set_str(&agent->laddr, "192.168.86.105", 0);
    }
    err = net_default_source_addr_get(AF_INET, &agent->laddr);
    if (err)
        goto out;

    err = udp_listen(&agent->us, &agent->laddr, 0, 0);
    if (err)
        goto out;

    err = udp_local_get(agent->us, &agent->laddr);
    if (err)
        goto out;

    lrole = offerer ? ICE_ROLE_CONTROLLING : ICE_ROLE_CONTROLLED;
    tiebrk = offerer ? 2 : 1;

    err = icem_alloc(&agent->icem, mode, lrole, IPPROTO_UDP, 0, tiebrk, agent->lufrag, agent->lpwd, agent_connchk_handler, agent);
    if (err)
        goto out;

#if 0
	icem_conf(agent->icem)->debug = true;
#endif

#if 0
	/* verify Mode and Role using debug strings (temp) */
	if (mode == ICE_MODE_FULL) {
		TEST_ASSERT(find_debug_string(agent->icem, "local_mode=Full"));
	}
	else {
		TEST_ASSERT(find_debug_string(agent->icem, "local_mode=Lite"));
	}
#endif

    if (offerer) {
        TEST_EQUALS(ICE_ROLE_CONTROLLING, icem_local_role(agent->icem));
    } else {
        TEST_EQUALS(ICE_ROLE_CONTROLLED, icem_local_role(agent->icem));
    }

    icem_set_name(agent->icem, name);

    err = icem_comp_add(agent->icem, compid, agent->us);
    if (err)
        goto out;

    err = icem_cand_add(agent->icem, compid, 0, "eth0", &agent->laddr);
    if (err)
        goto out;

    ++agent->n_cand;

    /* Start gathering now -- full mode only
	 *
	 * A lite implementation doesn't gather candidates;
	 * it includes only host candidates for any media stream.
	 */
    if (mode == ICE_MODE_FULL) {

        if (agent->use_turn) {

            re_printf("turn disabled\n");
        } else {
            err = icem_gather_srflx(agent, &agent->stun->laddr);
        }

        if (err)
            goto out;
    }

out:
    if (err)
        mem_deref(agent);
    else
        *agentp = agent;

    return err;
}

/*
 * ICE Test
 */

static int verify_after_sdp_exchange(struct agent *agent)
{
    struct agent *other = agent_other(agent);
    struct le *item;
    int err = 0;

    /* verify remote mode (after SDP exchange) */
    if (other->mode == ICE_MODE_FULL) {
        TEST_ASSERT(find_debug_string(agent->icem, "remote_mode=Full"));
    }

    /* verify ICE states */
    TEST_ASSERT(!icem_mismatch(agent->icem));

    /* after SDP was exchanged, we expect:
	 *   1 local candidate
	 *   1 remote candidates
	 *   checklist and validlist is empty
	 */
    //TEST_EQUALS(agent->n_cand, list_count(icem_lcandl(agent->icem)));
    //TEST_EQUALS(other->n_cand, list_count(icem_rcandl(agent->icem)));
    mn_log_info("%u local candidates", list_count(icem_lcandl(agent->icem)));
    mn_log_info("%u remote candidates", list_count(icem_rcandl(agent->icem)));

    //for (item = list_head(icem_rcandl(agent->icem)); item != NULL; item = item->next) {
    //    item->data
    //}

    TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
    TEST_EQUALS(0, list_count(icem_validl(agent->icem)));

    if (agent->use_turn) {
        /* verify that default candidate is the relayed address */
        //TEST_SACMP(&agent->turn->relay, icem_cand_default(agent->icem, agent->compid), SA_ALL);
    } else {
        /* verify that default candidate is our local address */
        //TEST_SACMP(&agent->laddr, icem_cand_default(agent->icem, agent->compid), SA_ALL);
    }

    /* we should not have selected candidate-pairs yet */
    TEST_ASSERT(!icem_selected_laddr(agent->icem, agent->compid));

out:
    if (err) {
        DEBUG_WARNING("agent %s failed\n", agent->name);
    }
    return err;
}

static int agent_start(struct agent *agent)
{
    struct agent *other = agent_other(agent);
    int err = 0;

    /* verify that check-list is empty before we start */
    RAWR_ASSERT(0 == list_count(icem_checkl(agent->icem)));
    RAWR_ASSERT(0 == list_count(icem_validl(agent->icem)));

    if (agent->mode == ICE_MODE_FULL) {

        err = icem_conncheck_start(agent->icem);
        if (err)
            return err;

        //TEST_EQUALS(agent->n_cand * other->n_cand, list_count(icem_checkl(agent->icem)));
        mn_log_info("Total Candidates: %u", list_count(icem_checkl(agent->icem)));
    }

    RAWR_ASSERT(0 == list_count(icem_validl(agent->icem)));

out:
    return err;
}

static int agent_verify_completed(struct agent *agent)
{
    struct agent *other = agent_other(agent);
    uint32_t validc;
    int err = 0;

    if (agent->mode == ICE_MODE_FULL) {
        TEST_ASSERT(agent->gathering_ok);
        TEST_ASSERT(agent->conncheck_ok);
    }

    TEST_EQUALS(0, list_count(icem_checkl(agent->icem)));
    validc = list_count(icem_validl(agent->icem));
    if (agent->mode == ICE_MODE_FULL) {
        TEST_EQUALS(agent->n_cand * other->n_cand, validc);
    }

    /* verify state of STUN/TURN server */
    if (agent->mode == ICE_MODE_FULL) {
        if (agent->use_turn) {
            TEST_ASSERT(agent->turn->n_allocate >= 1);
            TEST_ASSERT(agent->turn->n_chanbind >= 1);
        } else {
            TEST_ASSERT(agent->stun->nrecv >= 1);
        }
    }

out:
    return err;
}

static void icetest_check_gatherings(struct ice_test *it)
{
    int err;

    if (it->a->mode == ICE_MODE_FULL && !it->a->gathering_ok)
        return;
    if (it->b->mode == ICE_MODE_FULL && !it->b->gathering_ok)
        return;

    /* both gatherings are complete
	 * exchange SDP and start conncheck
	 */

    err = agent_decode_sdp(it->a, it->b);
    if (err)
        goto out;
    err = agent_decode_sdp(it->b, it->a);
    if (err)
        goto out;

    err = verify_after_sdp_exchange(it->a);
    if (err)
        goto error;
    err = verify_after_sdp_exchange(it->b);
    if (err)
        goto error;

    err = agent_start(it->a);
    if (err)
        goto out;
    err = agent_start(it->b);
    if (err)
        goto out;

    return;

out:
error:
    complete_test(it, err);
}

static void tmr_handler(void *arg)
{
    struct ice_test *it = arg;

#if 0
	re_printf("\n\x1b[32m%H\x1b[;m\n", icem_debug, it->a->icem);
	re_printf("\n\x1b[36m%H\x1b[;m\n", icem_debug, it->b->icem);
#endif

    complete_test(it, 0);
}

static void icetest_check_connchecks(struct ice_test *it)
{
    if (it->a->mode == ICE_MODE_FULL && !it->a->conncheck_ok)
        return;
    if (it->b->mode == ICE_MODE_FULL && !it->b->conncheck_ok)
        return;

    /* start an async timer to let the socket traffic complete */
    tmr_start(&it->tmr, 1, tmr_handler, it);
}

static void icetest_destructor(void *arg)
{
    struct ice_test *it = arg;

    tmr_cancel(&it->tmr);
    mem_deref(it->b);
    mem_deref(it->a);
}

static int icetest_alloc(struct ice_test **itp, enum ice_mode mode_a, bool turn_a, enum ice_mode mode_b, bool turn_b)
{
    struct ice_test *it;
    int err;

    it = mem_zalloc(sizeof(*it), icetest_destructor);
    if (!it)
        return ENOMEM;

    err = agent_alloc(&it->a, it, mode_a, turn_a, "A", 7, true);
    if (err)
        goto out;

    err = agent_alloc(&it->b, it, mode_b, turn_b, "B", 7, false);
    if (err)
        goto out;

out:
    if (err)
        mem_deref(it);
    else
        *itp = it;

    return err;
}

static int _test_ice_loop(enum ice_mode mode_a, bool turn_a, enum ice_mode mode_b, bool turn_b)
{
    struct ice_test *it = NULL;
    int err;

    err = icetest_alloc(&it, mode_a, turn_a, mode_b, turn_b);
    if (err)
        goto out;

    err = re_main_timeout(300);
    if (err)
        goto out;

    /* read back global errorcode */
    if (it->err) {
        err = it->err;
        goto out;
    }

    /* now verify all results after test was finished */
    err = agent_verify_completed(it->a);
    err |= agent_verify_completed(it->b);
    if (err)
        goto out;

out:
    mem_deref(it);

    return err;
}

/* also verify that these symbols are exported */
static int test_ice_basic_candidate(void)
{
    static const enum ice_cand_type typev[4] = {
        ICE_CAND_TYPE_HOST,
        ICE_CAND_TYPE_SRFLX,
        ICE_CAND_TYPE_PRFLX,
        ICE_CAND_TYPE_RELAY};
    unsigned i;
    int err = 0;

    for (i = 0; i < ARRAY_SIZE(typev); i++) {

        const char *name;
        enum ice_cand_type type;

        name = ice_cand_type2name(typev[i]);
        TEST_ASSERT(str_isset(name));

        type = ice_cand_name2type(name);
        TEST_EQUALS(typev[i], type);
    }

out:
    return err;
}

static int test_ice_cand_prio(void)
{
    int err = 0;

    TEST_EQUALS(0x7e0000ff, ice_cand_calc_prio(ICE_CAND_TYPE_HOST, 0, 1));
    TEST_EQUALS(0x640004ff, ice_cand_calc_prio(ICE_CAND_TYPE_SRFLX, 4, 1));
    TEST_EQUALS(0x6e0000fe, ice_cand_calc_prio(ICE_CAND_TYPE_PRFLX, 0, 2));
    TEST_EQUALS(0x000004fe, ice_cand_calc_prio(ICE_CAND_TYPE_RELAY, 4, 2));
out:
    return err;
}

static const char *testv[] = {
    "1 1 UDP 2130706431 10.0.1.1 5000 typ host",
    "1 2 UDP 2130706431 10.0.1.1 5001 typ host",
    "2 1 UDP 1694498815 192.0.2.3 5000 typ srflx raddr 10.0.1.1 rport 8998",
    "2 2 UDP 1694498815 192.0.2.3 5001 typ srflx raddr 10.0.1.1 rport 8998",

    "1 1 TCP 2128609279 10.0.1.1 9 typ host tcptype active",
    "2 1 TCP 2124414975 10.0.1.1 8998 typ host tcptype passive",
    "3 1 TCP 2120220671 10.0.1.1 8999 typ host tcptype so",
    "4 1 TCP 1688207359 192.0.2.3 9 typ srflx raddr"
    " 10.0.1.1 rport 9 tcptype active",
    "5 1 TCP 1684013055 192.0.2.3 45664 typ srflx raddr"
    " 10.0.1.1 rport 8998 tcptype passive",
    "6 1 TCP 1692401663 192.0.2.3 45687 typ srflx raddr"
    " 10.0.1.1 rport 8999 tcptype so",

#ifdef HAVE_INET6
    "H76f0ae12 1 UDP 2130706431 fda8:de2d:e95f:4811::1 6054 typ host",

    "3113280040 1 UDP 2122255103 2001:aaaa:5ef5:79fb:1847:2c0d:a230:23ab 53329"
    " typ host",
#endif

};

static int test_ice_cand_attribute(void)
{
    unsigned i;
    int err = 0;

    for (i = 0; i < ARRAY_SIZE(testv); i++) {

        struct ice_cand_attr cand;
        char buf[256];
        int n;

        err = ice_cand_attr_decode(&cand, testv[i]);
        TEST_ERR(err);

        /* sanity-check of decoded attribute */
        TEST_ASSERT(str_isset(cand.foundation));
        TEST_ASSERT(1 <= cand.compid && cand.compid <= 2);
        TEST_ASSERT(cand.proto == IPPROTO_UDP || cand.proto == IPPROTO_TCP);
        TEST_ASSERT(cand.prio > 0);
        TEST_ASSERT(sa_isset(&cand.addr, SA_ALL));

        n = re_snprintf(buf, sizeof(buf), "%H", ice_cand_attr_encode, &cand);
        if (n < 0)
            return ENOMEM;

        TEST_STRCMP(testv[i], strlen(testv[i]), buf, (unsigned)n);
    }

out:
    return err;
}

int test_ice_cand(void)
{
    int err = 0;

    err = test_ice_basic_candidate();
    if (err)
        return err;

    err = test_ice_cand_prio();
    if (err)
        return err;

    err = test_ice_cand_attribute();
    if (err)
        return err;

    return err;
}

int test_ice_loop(void)
{
    return _test_ice_loop(ICE_MODE_FULL, false, ICE_MODE_FULL, false);
}



enum {
    TCP_MAX_LENGTH = 2048,
};

static void process_msg(struct stunserver *stun, int proto, void *sock, const struct sa *src, const struct sa *dst, struct mbuf *mb)
{
    struct stun_msg *msg;
    bool fp = false;
    int err;
    (void)dst;

    stun->nrecv++;

    err = stun_msg_decode(&msg, mb, NULL);
    if (err)
        return;

	stun_msg_dump(msg);

    TEST_EQUALS(0x0001, stun_msg_type(msg));
    TEST_EQUALS(STUN_CLASS_REQUEST, stun_msg_class(msg));
    TEST_EQUALS(STUN_METHOD_BINDING, stun_msg_method(msg));

    /* mirror FINGERPRINT attribute back in response */
    fp = NULL != stun_msg_attr(msg, STUN_ATTR_FINGERPRINT);
    if (fp) {
        TEST_EQUALS(0, stun_msg_chk_fingerprint(msg));
    }

    err = stun_reply(proto, sock, src, 0, msg, NULL, 0, fp, 2, STUN_ATTR_MAPPED_ADDR, src, STUN_ATTR_XOR_MAPPED_ADDR, src);

out:
    if (err) {
        (void)stun_ereply(proto, sock, src, 0, msg, 400, "Bad Request", NULL, 0, fp, 0);
    }

    mem_deref(msg);
}

static void stunserver_udp_recv(const struct sa *src, struct mbuf *mb, void *arg)
{
    struct stunserver *stun = arg;

    process_msg(stun, IPPROTO_UDP, stun->us, src, &stun->laddr, mb);
}

static void tcp_recv(struct mbuf *mb, void *arg)
{
    struct stunserver *stun = arg;
    int err = 0;

    if (stun->mb) {
        size_t pos;

        pos = stun->mb->pos;

        stun->mb->pos = stun->mb->end;

        err = mbuf_write_mem(stun->mb, mbuf_buf(mb), mbuf_get_left(mb));
        if (err) {
            goto out;
        }

        stun->mb->pos = pos;
    } else {
        stun->mb = mem_ref(mb);
    }

    for (;;) {

        size_t len, pos, end;
        uint16_t typ;

        if (mbuf_get_left(stun->mb) < 4)
            break;

        typ = ntohs(mbuf_read_u16(stun->mb));
        len = ntohs(mbuf_read_u16(stun->mb));

        if (len > TCP_MAX_LENGTH) {
            DEBUG_WARNING("tcp: bad length: %zu\n", len);
            err = EBADMSG;
            goto out;
        }

        if (typ < 0x4000)
            len += STUN_HEADER_SIZE;
        else if (typ < 0x8000)
            len += 4;
        else {
            DEBUG_WARNING("tcp: bad type: 0x%04x\n", typ);
            err = EBADMSG;
            goto out;
        }

        stun->mb->pos -= 4;

        if (mbuf_get_left(stun->mb) < len)
            break;

        pos = stun->mb->pos;
        end = stun->mb->end;

        stun->mb->end = pos + len;

        process_msg(stun, IPPROTO_TCP, stun->tc, &stun->paddr, &stun->laddr_tcp, stun->mb);

        /* 4 byte alignment */
        while (len & 0x03)
            ++len;

        stun->mb->pos = pos + len;
        stun->mb->end = end;

        if (stun->mb->pos >= stun->mb->end) {
            stun->mb = mem_deref(stun->mb);
            break;
        }
    }

out:
    if (err) {
        stun->mb = mem_deref(stun->mb);
    }
}

static void tcp_close(int err, void *arg)
{
    struct stunserver *stun = arg;
    (void)err;

    stun->tc = mem_deref(stun->tc);
}

static void tcp_conn_handler(const struct sa *peer, void *arg)
{
    struct stunserver *stun = arg;
    int err;

    /* max 1 TCP connection */
    TEST_ASSERT(stun->tc == NULL);
    err = tcp_accept(&stun->tc, stun->ts, NULL, tcp_recv, tcp_close, stun);
    if (err)
        goto out;

    stun->paddr = *peer;

out:
    if (err) {
        /* save the error code */
        stun->err = err;

        tcp_reject(stun->ts);
    }
}

static void stunserver_destructor(void *arg)
{
    struct stunserver *stun = arg;

    mem_deref(stun->us);
    mem_deref(stun->mb);
    mem_deref(stun->tc);
    mem_deref(stun->ts);
}

/* Both UDP- and TCP-transport enabled by default */
int stunserver_alloc(struct stunserver **stunp)
{
    struct stunserver *stun;
    struct sa laddr;
    int err;

    if (!stunp)
        return EINVAL;

    stun = mem_zalloc(sizeof(*stun), stunserver_destructor);
    if (!stun)
        return ENOMEM;

    err = net_default_source_addr_get(AF_INET, &laddr);
    if (err)
        goto out;

    err = udp_listen(&stun->us, &laddr, stunserver_udp_recv, stun);
    if (err)
        goto out;

    //err = udp_local_get(stun->us, &stun->laddr);
    err = sa_set_str(&stun->laddr, "54.157.231.148", 3478);
    if (err)
        goto out;

    err = tcp_listen(&stun->ts, &laddr, tcp_conn_handler, stun);
    if (err)
        goto out;

    err = tcp_local_get(stun->ts, &stun->laddr_tcp);
    if (err)
        goto out;

#if 0
	DEBUG_NOTICE("stunserver: udp=%J, tcp=%J\n",
		     &stun->laddr, &stun->laddr_tcp);
#endif

out:
    if (err)
        mem_deref(stun);
    else
        *stunp = stun;

    return err;
}

const struct sa *stunserver_addr(const struct stunserver *stun, int proto)
{
    if (!stun)
        return NULL;

    switch (proto) {

    case IPPROTO_UDP:
        return &stun->laddr;
    case IPPROTO_TCP:
        return &stun->laddr_tcp;
    default:
        return NULL;
    }

    return NULL;
}


static void oom_watchdog_timeout(void *arg)
{
    int *err = arg;

    *err = ETIMEDOUT;

    re_cancel();
}

static uint32_t timeout_override;
int re_main_timeout(uint32_t timeout_ms)
{
    struct tmr tmr;
    int err = 0;

    tmr_init(&tmr);

    if (timeout_override != 0)
        timeout_ms = timeout_override;

#ifdef TEST_TIMEOUT
    timeout_ms = TEST_TIMEOUT;
#endif

    tmr_start(&tmr, timeout_ms, oom_watchdog_timeout, &err);
    (void)re_main(NULL);

    tmr_cancel(&tmr);
    return err;
}



int main(void)
{
    libre_init();
    test_ice_loop();
    test_ice_cand();
    libre_close();
    return 0;
}
