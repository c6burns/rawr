#include "re.h"

#include "mn/log.h"
#include "mn/thread.h"

/**
 * Check if an SDP media object has valid media. It is considered
 * valid if it has one or more codecs, and the port number is set.
 *
 * @param m SDP Media object
 *
 * @return True if it has media, false if not
 */
bool sdp_media_has_media(const struct sdp_media *m)
{
    bool has;

    has = sdp_media_rformat(m, NULL) != NULL;
    if (has)
        return sdp_media_rport(m) != 0;

    return false;
}

/*
 * STUN URI
 */

/** Defines the STUN uri scheme */
enum stun_scheme {
    STUN_SCHEME_STUN,  /**< STUN scheme        */
    STUN_SCHEME_STUNS, /**< Secure STUN scheme */
    STUN_SCHEME_TURN,  /**< TURN scheme        */
    STUN_SCHEME_TURNS, /**< Secure TURN scheme */
};

/** Defines a STUN/TURN uri */
struct stun_uri {
    enum stun_scheme scheme; /**< STUN Scheme            */
    char *host;              /**< Hostname or IP-address */
    uint16_t port;           /**< Port number            */
};

int stunuri_decode(struct stun_uri **sup, const struct pl *pl);
int stunuri_set_host(struct stun_uri *su, const char *host);
int stunuri_set_port(struct stun_uri *su, uint16_t port);
int stunuri_print(struct re_printf *pf, const struct stun_uri *su);
const char *stunuri_scheme_name(enum stun_scheme scheme);

/*
  https://tools.ietf.org/html/rfc7064
  https://tools.ietf.org/html/rfc7065


                          +-----------------------+
                          | URI                   |
                          +-----------------------+
                          | stun:example.org      |
                          | stuns:example.org     |
                          | stun:example.org:8000 |
                          +-----------------------+


   +---------------------------------+----------+--------+-------------+
   | URI                             | <secure> | <port> | <transport> |
   +---------------------------------+----------+--------+-------------+
   | turn:example.org                | false    |        |             |
   | turns:example.org               | true     |        |             |
   | turn:example.org:8000           | false    | 8000   |             |
   | turn:example.org?transport=udp  | false    |        | UDP         |
   | turn:example.org?transport=tcp  | false    |        | TCP         |
   | turns:example.org?transport=tcp | true     |        | TLS         |
   +---------------------------------+----------+--------+-------------+

 */

static void destructor(void *arg)
{
    struct stun_uri *su = arg;

    mem_deref(su->host);
}

/**
 * Decode a STUN uri from a string
 *
 * @param sup Pointer to allocated STUN uri
 * @param pl  Pointer-length string
 *
 * @return 0 if success, otherwise errorcode
 */
int stunuri_decode(struct stun_uri **sup, const struct pl *pl)
{
    struct stun_uri *su;
    struct uri uri;
    enum stun_scheme scheme;
    int err;

    if (!sup || !pl)
        return EINVAL;

    err = uri_decode(&uri, pl);
    if (err) {
        mn_log_warn("stunuri: decode '%r' failed (%m)", pl, err);
        return err;
    }

    if (0 == pl_strcasecmp(&uri.scheme, "stun"))
        scheme = STUN_SCHEME_STUN;
    else if (0 == pl_strcasecmp(&uri.scheme, "stuns"))
        scheme = STUN_SCHEME_STUNS;
    else if (0 == pl_strcasecmp(&uri.scheme, "turn"))
        scheme = STUN_SCHEME_TURN;
    else if (0 == pl_strcasecmp(&uri.scheme, "turns"))
        scheme = STUN_SCHEME_TURNS;
    else {
        mn_log_warn("stunuri: scheme not supported (%r)", &uri.scheme);
        return ENOTSUP;
    }

    su = mem_zalloc(sizeof(*su), destructor);
    if (!su)
        return ENOMEM;

    su->scheme = scheme;
    err = pl_strdup(&su->host, &uri.host);
    su->port = uri.port;

    if (err)
        mem_deref(su);
    else
        *sup = su;

    return err;
}

/**
 * Set the hostname on a STUN uri
 *
 * @param su   STUN uri
 * @param host Hostname to set
 *
 * @return 0 if success, otherwise errorcode
 */
int stunuri_set_host(struct stun_uri *su, const char *host)
{
    if (!su || !host)
        return EINVAL;

    su->host = mem_deref(su->host);

    return str_dup(&su->host, host);
}

/**
 * Set the port number on a STUN uri
 *
 * @param su   STUN uri
 * @param port Port number to set
 *
 * @return 0 if success, otherwise errorcode
 */
int stunuri_set_port(struct stun_uri *su, uint16_t port)
{
    if (!su)
        return EINVAL;

    su->port = port;

    return 0;
}

/**
 * Print a STUN uri
 *
 * @param pf Print function
 * @param su STUN uri
 *
 * @return 0 if success, otherwise errorcode
 */
int stunuri_print(struct re_printf *pf, const struct stun_uri *su)
{
    int err = 0;

    if (!su)
        return 0;

    err |= re_hprintf(pf, "scheme=%s", stunuri_scheme_name(su->scheme));
    err |= re_hprintf(pf, " host='%s'", su->host);
    err |= re_hprintf(pf, " port=%u", su->port);

    return err;
}

/**
 * Get the name of the STUN scheme
 *
 * @param scheme STUN scheme
 *
 * @return String with STUN scheme
 */
const char *stunuri_scheme_name(enum stun_scheme scheme)
{
    switch (scheme) {

    case STUN_SCHEME_STUN:
        return "stun";
    case STUN_SCHEME_STUNS:
        return "stuns";
    case STUN_SCHEME_TURN:
        return "turn";
    case STUN_SCHEME_TURNS:
        return "turns";
    default:
        return "?";
    }
}



struct mnat;
struct mnat_sess;
struct mnat_media;

typedef void(mnat_estab_h)(int err, uint16_t scode, const char *reason, void *arg);

typedef void(mnat_connected_h)(const struct sa *raddr1, const struct sa *raddr2, void *arg);

typedef int(mnat_sess_h)(struct mnat_sess **sessp, const struct mnat *mnat, struct dnsc *dnsc, int af, const struct stun_uri *srv, const char *user, const char *pass, struct sdp_session *sdp, bool offerer, mnat_estab_h *estabh, void *arg);

typedef int(mnat_media_h)(struct mnat_media **mp, struct mnat_sess *sess, struct udp_sock *sock1, struct udp_sock *sock2, struct sdp_media *sdpm, mnat_connected_h *connh, void *arg);

typedef int(mnat_update_h)(struct mnat_sess *sess);

struct mnat {
    struct le le;
    const char *id;
    const char *ftag;
    bool wait_connected;
    mnat_sess_h *sessh;
    mnat_media_h *mediah;
    mnat_update_h *updateh;
};

void mnat_register(struct list *mnatl, struct mnat *mnat);
void mnat_unregister(struct mnat *mnat);
const struct mnat *mnat_find(const struct list *mnatl, const char *id);

/**
 * Register a Media NAT traversal module
 *
 * @param mnatl    List of Media-NAT modules
 * @param mnat     Media NAT traversal module
 */
void mnat_register(struct list *mnatl, struct mnat *mnat)
{
    if (!mnatl || !mnat)
        return;

    list_append(mnatl, &mnat->le, mnat);

    mn_log_info("medianat: %s", mnat->id);
}

/**
 * Unregister a Media NAT traversal module
 *
 * @param mnat     Media NAT traversal module
 */
void mnat_unregister(struct mnat *mnat)
{
    if (!mnat)
        return;

    list_unlink(&mnat->le);
}

/**
 * Find a Media NAT module by name
 *
 * @param mnatl List of Media-NAT modules
 * @param id    Name of the Media NAT module to find
 *
 * @return Matching Media NAT module if found, otherwise NULL
 */
const struct mnat *mnat_find(const struct list *mnatl, const char *id)
{
    struct mnat *mnat;
    struct le *le;

    if (!mnatl)
        return NULL;

    for (le = mnatl->head; le; le = le->next) {

        mnat = le->data;

        if (str_casecmp(mnat->id, id))
            continue;

        return mnat;
    }

    return NULL;
}

enum {
    ICE_LAYER = 0
};

struct mnat_sess {
    struct list medial;
    struct sa srv;
    struct stun_dns *dnsq;
    struct sdp_session *sdp;
    struct tmr tmr_async;
    char lufrag[8];
    char lpwd[32];
    uint64_t tiebrk;
    bool turn;
    bool offerer;
    char *user;
    char *pass;
    bool started;
    bool send_reinvite;
    mnat_estab_h *estabh;
    void *arg;
};

struct mnat_media {
    struct comp {
        struct mnat_media *m; /* pointer to parent */
        struct stun_ctrans *ct_gath;
        struct sa laddr;
        unsigned id;
        void *sock;
    } compv[2];
    struct le le;
    struct mnat_sess *sess;
    struct sdp_media *sdpm;
    struct icem *icem;
    bool gathered;
    bool complete;
    bool terminated;
    int nstun; /**< Number of pending STUN candidates  */
    mnat_connected_h *connh;
    void *arg;
};

static void gather_handler(int err, uint16_t scode, const char *reason, void *arg);

static void call_gather_handler(int err, struct mnat_media *m, uint16_t scode, const char *reason)
{

    /* No more pending requests? */
    if (m->nstun != 0)
        return;

    mn_log_trace("ice: all components gathered.");

    if (err)
        goto out;

    /* Eliminate redundant local candidates */
    icem_cand_redund_elim(m->icem);

    err = icem_comps_set_default_cand(m->icem);
    if (err) {
        mn_log_warn("ice: set default cands failed (%m)", err);
        goto out;
    }

out:
    gather_handler(err, scode, reason, m);
}

static void stun_resp_handler(int err, uint16_t scode, const char *reason, const struct stun_msg *msg, void *arg)
{
    struct comp *comp = arg;
    struct mnat_media *m = comp->m;
    struct stun_attr *attr;
    struct ice_cand *lcand;

    if (m->terminated)
        return;

    --m->nstun;

    if (err || scode > 0) {
        mn_log_warn("ice: comp %u: STUN Request failed: %m", comp->id, err);
        goto out;
    }

    mn_log_trace("ice: srflx gathering for comp %u complete.", comp->id);

    /* base candidate */
    lcand = icem_cand_find(icem_lcandl(m->icem), comp->id, NULL);
    if (!lcand)
        goto out;

    attr = stun_msg_attr(msg, STUN_ATTR_XOR_MAPPED_ADDR);
    if (!attr)
        attr = stun_msg_attr(msg, STUN_ATTR_MAPPED_ADDR);
    if (!attr) {
        mn_log_warn("ice: no Mapped Address in Response");
        err = EPROTO;
        goto out;
    }

    err = icem_lcand_add(m->icem, icem_lcand_base(lcand), ICE_CAND_TYPE_SRFLX, &attr->v.sa);

out:
    call_gather_handler(err, m, scode, reason);
}

/** Gather Server Reflexive address */
static int send_binding_request(struct mnat_media *m, struct comp *comp)
{
    int err;

    if (comp->ct_gath)
        return EALREADY;

    mn_log_trace("ice: gathering srflx for comp %u ..", comp->id);

    err = stun_request(&comp->ct_gath, icem_stun(m->icem), IPPROTO_UDP, comp->sock, &m->sess->srv, 0, STUN_METHOD_BINDING, NULL, false, 0, stun_resp_handler, comp, 1, STUN_ATTR_SOFTWARE, stun_software);
    if (err)
        return err;

    ++m->nstun;

    return 0;
}

static void turnc_handler(int err, uint16_t scode, const char *reason, const struct sa *relay, const struct sa *mapped, const struct stun_msg *msg, void *arg)
{
    struct comp *comp = arg;
    struct mnat_media *m = comp->m;
    struct ice_cand *lcand;
    (void)msg;

    --m->nstun;

    /* TURN failed, so we destroy the client */
    if (err || scode) {
        icem_set_turn_client(m->icem, comp->id, NULL);
    }

    if (err) {
        mn_log_warn("{%u} TURN Client error: %m", comp->id, err);
        goto out;
    }

    if (scode) {
        mn_log_warn("{%u} TURN Client error: %u %s", comp->id, scode, reason);
        err = send_binding_request(m, comp);
        if (err)
            goto out;
        return;
    }

    mn_log_trace("ice: relay gathered for comp %u (%u %s)", comp->id, scode, reason);

    lcand = icem_cand_find(icem_lcandl(m->icem), comp->id, NULL);
    if (!lcand)
        goto out;

    if (!sa_cmp(relay, icem_lcand_addr(icem_lcand_base(lcand)), SA_ALL)) {
        err = icem_lcand_add(m->icem, icem_lcand_base(lcand), ICE_CAND_TYPE_RELAY, relay);
    }

    if (mapped) {
        err |= icem_lcand_add(m->icem, icem_lcand_base(lcand), ICE_CAND_TYPE_SRFLX, mapped);
    } else {
        err |= send_binding_request(m, comp);
    }

out:
    call_gather_handler(err, m, scode, reason);
}

static int cand_gather_relayed(struct mnat_media *m, struct comp *comp, const char *username, const char *password)
{
    struct turnc *turnc = NULL;
    const int layer = ICE_LAYER - 10; /* below ICE stack */
    int err;

    err = turnc_alloc(&turnc, stun_conf(icem_stun(m->icem)), IPPROTO_UDP, comp->sock, layer, &m->sess->srv, username, password, 60, turnc_handler, comp);
    if (err)
        return err;

    err = icem_set_turn_client(m->icem, comp->id, turnc);
    if (err)
        goto out;

    ++m->nstun;

out:
    mem_deref(turnc);

    return err;
}

static int start_gathering(struct mnat_media *m, const char *username, const char *password)
{
    unsigned i;
    int err = 0;

    /* for each component */
    for (i = 0; i < 2; i++) {
        struct comp *comp = &m->compv[i];

        if (!comp->sock)
            continue;

        if (m->sess->turn) {
            err |= cand_gather_relayed(m, comp, username, password);
        } else
            err |= send_binding_request(m, comp);
    }

    return err;
}

static int icem_gather_srflx(struct mnat_media *m)
{
    if (!m)
        return EINVAL;

    return start_gathering(m, NULL, NULL);
}

static int icem_gather_relay(struct mnat_media *m, const char *username, const char *password)
{
    if (!m || !username || !password)
        return EINVAL;

    return start_gathering(m, username, password);
}

static void ice_printf(struct mnat_media *m, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mn_log_trace("%s: %v", m ? sdp_media_name(m->sdpm) : "ICE", fmt, &ap);
    va_end(ap);
}

static void session_destructor(void *arg)
{
    struct mnat_sess *sess = arg;

    tmr_cancel(&sess->tmr_async);
    list_flush(&sess->medial);
    mem_deref(sess->dnsq);
    mem_deref(sess->user);
    mem_deref(sess->pass);
    mem_deref(sess->sdp);
}

static void media_destructor(void *arg)
{
    struct mnat_media *m = arg;
    unsigned i;

    m->terminated = true;

    list_unlink(&m->le);
    mem_deref(m->sdpm);
    mem_deref(m->icem);
    for (i = 0; i < 2; i++) {
        mem_deref(m->compv[i].ct_gath);
        mem_deref(m->compv[i].sock);
    }
}

static bool candidate_handler(struct le *le, void *arg)
{
    return 0 != sdp_media_set_lattr(arg, false, ice_attr_cand, "%H", ice_cand_encode, le->data);
}

/**
 * Update the local SDP attributes, this can be called multiple times
 * when the state of the ICE machinery changes
 */
static int set_media_attributes(struct mnat_media *m)
{
    int err = 0;

    if (icem_mismatch(m->icem)) {
        err = sdp_media_set_lattr(m->sdpm, true, ice_attr_mismatch, NULL);
        return err;
    } else {
        sdp_media_del_lattr(m->sdpm, ice_attr_mismatch);
    }

    /* Encode all my candidates */
    sdp_media_del_lattr(m->sdpm, ice_attr_cand);
    if (list_apply(icem_lcandl(m->icem), true, candidate_handler, m->sdpm))
        return ENOMEM;

    if (ice_remotecands_avail(m->icem)) {
        err |= sdp_media_set_lattr(m->sdpm, true, ice_attr_remote_cand, "%H", ice_remotecands_encode, m->icem);
    }

    return err;
}

static bool if_handler(const char *ifname, const struct sa *sa, void *arg)
{
    struct mnat_media *m = arg;
    uint16_t lprio;
    unsigned i;
    int err = 0;

    /* Skip loopback and link-local addresses */
    if (sa_is_loopback(sa) || sa_is_linklocal(sa))
        return false;

    //if (!net_af_enabled(baresip_network(), sa_af(sa)))
    //    return false;

    lprio = 0;

    ice_printf(m, "added interface: %s:%j (local prio %u)", ifname, sa, lprio);

    for (i = 0; i < 2; i++) {
        if (m->compv[i].sock)
            err |= icem_cand_add(m->icem, i + 1, lprio, ifname, sa);
    }

    if (err) {
        mn_log_warn("ice: %s:%j: icem_cand_add: %m", ifname, sa, err);
    }

    return false;
}

static int media_start(struct mnat_sess *sess, struct mnat_media *m)
{
    int err = 0;

    net_if_apply(if_handler, m);

    if (sess->turn) {
        err = icem_gather_relay(m, sess->user, sess->pass);
    } else {
        err = icem_gather_srflx(m);
    }

    return err;
}

static void dns_handler(int err, const struct sa *srv, void *arg)
{
    struct mnat_sess *sess = arg;
    struct le *le;

    if (err)
        goto out;

    mn_log_trace("ice: resolved %s-server to address %J", sess->turn ? "TURN" : "STUN", srv);

    sess->srv = *srv;

    for (le = sess->medial.head; le; le = le->next) {

        struct mnat_media *m = le->data;

        err = media_start(sess, m);
        if (err)
            goto out;
    }

    return;

out:
    sess->estabh(err, 0, NULL, sess->arg);
}

static void tmr_async_handler(void *arg)
{
    struct mnat_sess *sess = arg;
    struct le *le;

    for (le = sess->medial.head; le; le = le->next) {

        struct mnat_media *m = le->data;

        net_if_apply(if_handler, m);

        call_gather_handler(0, m, 0, "");
    }
}

static int session_alloc(struct mnat_sess **sessp, const struct mnat *mnat, struct dnsc *dnsc, int af, const struct stun_uri *srv, const char *user, const char *pass, struct sdp_session *ss, bool offerer, mnat_estab_h *estabh, void *arg)
{
    struct mnat_sess *sess;
    const char *usage = NULL;
    int err = 0;
    (void)mnat;

    if (!sessp || !dnsc || !ss || !estabh)
        return EINVAL;

    if (srv) {
        mn_log_info("ice: new session with %s-server at %s (username=%s)", srv->scheme == STUN_SCHEME_TURN ? "TURN" : "STUN", srv->host, user);

        switch (srv->scheme) {

        case STUN_SCHEME_STUN:
            usage = stun_usage_binding;
            break;

        case STUN_SCHEME_TURN:
            usage = stun_usage_relay;
            break;

        default:
            return ENOTSUP;
        }
    }

    sess = mem_zalloc(sizeof(*sess), session_destructor);
    if (!sess)
        return ENOMEM;

    sess->sdp = mem_ref(ss);
    sess->estabh = estabh;
    sess->arg = arg;

    if (user && pass) {
        err = str_dup(&sess->user, user);
        err |= str_dup(&sess->pass, pass);
        if (err)
            goto out;
    }

    rand_str(sess->lufrag, sizeof(sess->lufrag));
    rand_str(sess->lpwd, sizeof(sess->lpwd));
    sess->tiebrk = rand_u64();
    sess->offerer = offerer;

    err |= sdp_session_set_lattr(ss, true, ice_attr_ufrag, sess->lufrag);
    err |= sdp_session_set_lattr(ss, true, ice_attr_pwd, sess->lpwd);
    if (err)
        goto out;

    if (srv) {
        sess->turn = (srv->scheme == STUN_SCHEME_TURN);

        err = stun_server_discover(&sess->dnsq, dnsc, usage, stun_proto_udp, af, srv->host, srv->port, dns_handler, sess);
    } else {
        tmr_start(&sess->tmr_async, 1, tmr_async_handler, sess);
    }

out:
    if (err)
        mem_deref(sess);
    else
        *sessp = sess;

    return err;
}

static bool verify_peer_ice(struct mnat_sess *ms)
{
    struct le *le;

    for (le = ms->medial.head; le; le = le->next) {
        struct mnat_media *m = le->data;
        struct sa raddr[2];
        unsigned i;

        if (!sdp_media_has_media(m->sdpm)) {
            mn_log_info("ice: stream '%s' is disabled -- ignore", sdp_media_name(m->sdpm));
            continue;
        }

        raddr[0] = *sdp_media_raddr(m->sdpm);
        sdp_media_raddr_rtcp(m->sdpm, &raddr[1]);

        for (i = 0; i < 2; i++) {
            if (m->compv[i].sock &&
                !icem_verify_support(m->icem, i + 1, &raddr[i])) {
                mn_log_warn("ice: %s.%u: no remote candidates"
                        " found (address = %J)",
                        sdp_media_name(m->sdpm),
                        i + 1,
                        &raddr[i]);
                return false;
            }
        }
    }

    return true;
}

static bool refresh_comp_laddr(struct mnat_media *m, unsigned id, struct comp *comp, const struct sa *laddr)
{
    bool changed = false;

    if (!m || !comp || !comp->sock || !laddr)
        return false;

    if (!sa_cmp(&comp->laddr, laddr, SA_ALL)) {
        changed = true;

        ice_printf(m, "comp%u setting local: %J", id, laddr);
    }

    sa_cpy(&comp->laddr, laddr);

    if (id == 1)
        sdp_media_set_laddr(m->sdpm, &comp->laddr);
    else if (id == 2)
        sdp_media_set_laddr_rtcp(m->sdpm, &comp->laddr);

    return changed;
}

/*
 * Update SDP Media with local addresses
 */
static bool refresh_laddr(struct mnat_media *m, const struct sa *laddr1, const struct sa *laddr2)
{
    bool changed = false;

    changed |= refresh_comp_laddr(m, 1, &m->compv[0], laddr1);
    changed |= refresh_comp_laddr(m, 2, &m->compv[1], laddr2);

    return changed;
}

static bool all_gathered(const struct mnat_sess *sess)
{
    struct le *le;

    for (le = sess->medial.head; le; le = le->next) {

        struct mnat_media *m = le->data;

        if (!m->gathered)
            return false;
    }

    return true;
}

static bool all_completed(const struct mnat_sess *sess)
{
    struct le *le;

    /* Check all conncheck flags */
    LIST_FOREACH(&sess->medial, le)
    {
        struct mnat_media *mx = le->data;
        if (!mx->complete)
            return false;
    }

    return true;
}

static void gather_handler(int err, uint16_t scode, const char *reason, void *arg)
{
    struct mnat_media *m = arg;
    mnat_estab_h *estabh = m->sess->estabh;

    if (err || scode) {
        mn_log_warn("ice: gather error: %m (%u %s)", err, scode, reason);
    } else {
        refresh_laddr(m, icem_cand_default(m->icem, 1), icem_cand_default(m->icem, 2));

        mn_log_info("ice: %s: Default local candidates: %J / %J", sdp_media_name(m->sdpm), &m->compv[0].laddr, &m->compv[1].laddr);

        (void)set_media_attributes(m);

        m->gathered = true;

        if (!all_gathered(m->sess))
            return;
    }

    if (err || scode)
        m->sess->estabh = NULL;

    if (estabh)
        estabh(err, scode, reason, m->sess->arg);
}

static void conncheck_handler(int err, bool update, void *arg)
{
    struct mnat_media *m = arg;
    struct mnat_sess *sess = m->sess;
    bool sess_complete = false;

    mn_log_info("ice: %s: connectivity check is complete (update=%d)", sdp_media_name(m->sdpm), update);

    ice_printf(m, "Dumping media state: %H", icem_debug, m->icem);

    if (err) {
        mn_log_warn("ice: connectivity check failed: %m", err);
    } else {
        const struct ice_cand *cand1, *cand2;
        bool changed;

        m->complete = true;

        changed = refresh_laddr(m, icem_selected_laddr(m->icem, 1), icem_selected_laddr(m->icem, 2));
        if (changed)
            sess->send_reinvite = true;

        (void)set_media_attributes(m);

        cand1 = icem_selected_rcand(m->icem, 1);
        cand2 = icem_selected_rcand(m->icem, 2);

        sess_complete = all_completed(sess);

        if (m->connh) {
            m->connh(icem_lcand_addr(cand1), icem_lcand_addr(cand2), m->arg);
        }
    }

    /* call estab-handler and send re-invite */
    if (sess_complete && sess->send_reinvite && update) {

        mn_log_info("ice: %s: sending Re-INVITE with updated"
             " default candidates",
             sdp_media_name(m->sdpm));

        sess->send_reinvite = false;
        sess->estabh(0, 0, NULL, sess->arg);
    }
}

static int ice_start(struct mnat_sess *sess)
{
    struct le *le;
    int err = 0;

    /* Update SDP media */
    if (sess->started) {

        LIST_FOREACH(&sess->medial, le)
        {
            struct mnat_media *m = le->data;

            ice_printf(NULL, "ICE Start: %H", icem_debug, m->icem);

            icem_update(m->icem);

            refresh_laddr(m, icem_selected_laddr(m->icem, 1), icem_selected_laddr(m->icem, 2));

            err |= set_media_attributes(m);
        }

        return err;
    }

    /* Clear all conncheck flags */
    LIST_FOREACH(&sess->medial, le)
    {
        struct mnat_media *m = le->data;

        if (sdp_media_has_media(m->sdpm)) {
            m->complete = false;

            err = icem_conncheck_start(m->icem);
            if (err)
                return err;

            /* set the pair states
			   -- first media stream only */
            if (sess->medial.head == le) {
                ice_candpair_set_states(m->icem);
            }
        } else {
            m->complete = true;
        }
    }

    sess->started = true;

    return 0;
}

static int media_alloc(struct mnat_media **mp, struct mnat_sess *sess, struct udp_sock *sock1, struct udp_sock *sock2, struct sdp_media *sdpm, mnat_connected_h *connh, void *arg)
{
    struct mnat_media *m;
    enum ice_role role;
    unsigned i;
    int err = 0;

    if (!mp || !sess || !sdpm)
        return EINVAL;

    m = mem_zalloc(sizeof(*m), media_destructor);
    if (!m)
        return ENOMEM;

    list_append(&sess->medial, &m->le, m);
    m->sdpm = mem_ref(sdpm);
    m->sess = sess;
    m->compv[0].sock = mem_ref(sock1);
    m->compv[1].sock = mem_ref(sock2);

    if (sess->offerer)
        role = ICE_ROLE_CONTROLLING;
    else
        role = ICE_ROLE_CONTROLLED;

    err = icem_alloc(&m->icem, ICE_MODE_FULL, role, IPPROTO_UDP, ICE_LAYER, sess->tiebrk, sess->lufrag, sess->lpwd, conncheck_handler, m);
    if (err)
        goto out;

    icem_conf(m->icem)->debug = 1;
    icem_conf(m->icem)->rc = 4;

    icem_set_conf(m->icem, icem_conf(m->icem));

    icem_set_name(m->icem, sdp_media_name(sdpm));

    for (i = 0; i < 2; i++) {
        m->compv[i].m = m;
        m->compv[i].id = i + 1;
        if (m->compv[i].sock)
            err |= icem_comp_add(m->icem, i + 1, m->compv[i].sock);
    }

    m->connh = connh;
    m->arg = arg;

    if (sa_isset(&sess->srv, SA_ALL))
        err |= media_start(sess, m);

out:
    if (err)
        mem_deref(m);
    else {
        *mp = m;
    }

    return err;
}

static bool sdp_attr_handler(const char *name, const char *value, void *arg)
{
    struct mnat_sess *sess = arg;
    struct le *le;

    for (le = sess->medial.head; le; le = le->next) {
        struct mnat_media *m = le->data;

        (void)ice_sdp_decode(m->icem, name, value);
    }

    return false;
}

static bool media_attr_handler(const char *name, const char *value, void *arg)
{
    struct mnat_media *m = arg;
    return 0 != icem_sdp_decode(m->icem, name, value);
}

static int enable_turn_channels(struct mnat_sess *sess)
{
    struct le *le;
    int err = 0;

    for (le = sess->medial.head; le; le = le->next) {

        struct mnat_media *m = le->data;
        struct sa raddr[2];
        unsigned i;

        err |= set_media_attributes(m);

        raddr[0] = *sdp_media_raddr(m->sdpm);
        sdp_media_raddr_rtcp(m->sdpm, &raddr[1]);

        for (i = 0; i < 2; i++) {
            if (m->compv[i].sock && sa_isset(&raddr[i], SA_ALL))
                err |= icem_add_chan(m->icem, i + 1, &raddr[i]);
        }
    }

    return err;
}

/** This can be called several times */
static int update(struct mnat_sess *sess)
{
    struct le *le;
    int err = 0;

    if (!sess)
        return EINVAL;

    /* SDP session */
    (void)sdp_session_rattr_apply(sess->sdp, NULL, sdp_attr_handler, sess);

    /* SDP medialines */
    for (le = sess->medial.head; le; le = le->next) {
        struct mnat_media *m = le->data;

        sdp_media_rattr_apply(m->sdpm, NULL, media_attr_handler, m);
    }

    /* 5.1.  Verifying ICE Support */
    if (verify_peer_ice(sess)) {
        err = ice_start(sess);
    } else if (sess->turn) {
        mn_log_info("ice: ICE not supported by peer, fallback to TURN");
        err = enable_turn_channels(sess);
    } else {
        mn_log_info("ice: ICE not supported by peer");

        LIST_FOREACH(&sess->medial, le)
        {
            struct mnat_media *m = le->data;

            err |= set_media_attributes(m);
        }
    }

    return err;
}


static struct mnat mnat_ice = {
    .id = "ice",
    .ftag = "+sip.ice",
    .wait_connected = true,
    .sessh = session_alloc,
    .mediah = media_alloc,
    .updateh = update,
};
/*
static int module_init(void)
{
    mnat_register(baresip_mnatl(), &mnat_ice);

    return 0;
}

static int module_close(void)
{
    mnat_unregister(&mnat_ice);

    return 0;
}

EXPORT_SYM const struct mod_export DECL_EXPORTS(ice) = {
    "ice",
    "mnat",
    module_init,
    module_close,
};
*/

static struct sipsess_sock *re_sess_sock; /* SIP session socket */
static struct sdp_session *re_sdp;        /* SDP session        */
static struct sdp_media *re_sdp_media;    /* SDP media          */
static struct sipsess *re_sess;           /* SIP session        */
static struct sipreg *re_reg;             /* SIP registration   */
static struct sip *re_sip;                /* SIP stack          */
static struct rtp_sock *re_rtp;           /* RTP socket         */

static void rtp_handler(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *arg)
{
    (void)hdr;
    (void)arg;

    re_printf("rtp: recv %zu bytes from %J\n", mbuf_get_left(mb), src);
}

static void handle_nat_connected(const struct sa *raddr1, const struct sa *raddr2, void *arg)
{
    int abc = 123;
}

static void handle_udp_recv(const struct sa *src, struct mbuf *mb, void *arg)
{
    int abc = 123;
}

int main(void)
{
    struct sa nsv[16];
    struct dnsc *dnsc = NULL;
    struct sa laddr;
    uint32_t nsc;
    int err; /* errno return values */

    /* enable coredumps to aid debugging */
    (void)sys_coredump_set(true);

    /* initialize libre state */
    err = libre_init();
    if (err) {
        mn_log_error("re init failed: %s", strerror(err));
        goto out;
    }

    nsc = ARRAY_SIZE(nsv);

    /* fetch list of DNS server IP addresses */
    err = dns_srv_get(NULL, 0, nsv, &nsc);
    if (err) {
        mn_log_error("unable to get dns servers: %s", strerror(err));
        goto out;
    }

    /* create DNS client */
    err = dnsc_alloc(&dnsc, NULL, nsv, nsc);
    if (err) {
        mn_log_error("unable to create dns client: %s", strerror(err));
        goto out;
    }

    /* fetch local IP address */
    err = net_default_source_addr_get(AF_INET, &laddr);
    if (err) {
        mn_log_error("local address error: %s", strerror(err));
        goto out;
    }

    mn_log_info("local address: %s", inet_ntoa(laddr.u.in.sin_addr));

    /* listen on random port */
    sa_set_port(&laddr, 0);

    /* create the RTP/RTCP socket */
    err = rtp_listen(&re_rtp, IPPROTO_UDP, &laddr, 16384, 32767, true, rtp_handler, NULL, NULL);
    if (err) {
        re_fprintf(stderr, "rtp listen error: %m\n", err);
        goto out;
    }

    re_printf("local RTP port is %u\n", sa_port(rtp_local(re_rtp)));

    /* create SDP session */
    err = sdp_session_alloc(&re_sdp, &laddr);
    if (err) {
        mn_log_error("sdp session error: %s", strerror(err));
        goto out;
    }

    /* add audio sdp media, using port from RTP socket */
    err = sdp_media_add(&re_sdp_media, re_sdp, "audio", sa_port(rtp_local(re_rtp)), "RTP/AVP");
    if (err) {
        mn_log_error("sdp media error: %s", strerror(err));
        goto out;
    }

    /* add G.711 sdp media format */
    err = sdp_format_add(NULL, re_sdp_media, false, "102", "opus", 48000, 2, NULL, NULL, NULL, false, NULL);
    if (err) {
        mn_log_error("sdp format error: %s", strerror(err));
        goto out;
    }

    struct mnat_sess *sess;
    struct mnat_media *media;
    struct stun_uri stun_server = {0};
    stunuri_set_host(&stun_server, "stun.l.google.com");
    stunuri_set_port(&stun_server, 19302);
    struct udp_sock *sock1 = NULL, *sock2 = NULL;

    udp_listen(sock1, &laddr, NULL, NULL);
    udp_listen(sock2, &laddr, NULL, NULL);

    session_alloc(&sess, &mnat_ice, dnsc, AF_INET, &stun_server, "", "", re_sdp, false, gather_handler, NULL);
    media_alloc(&media, sess, sock1, sock2, re_sdp_media, handle_nat_connected, NULL);
    while (1) {
        mn_thread_sleep_s(1);
        update(sess);
    }

    return 0;

out:
    mn_log_error("error!");
    return 1;
}
