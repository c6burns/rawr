#
#
# folder organization
macro(rawr_target_folder target_name folder_name)
    if (TARGET ${target_name})
        set_target_properties(${target_name} PROPERTIES FOLDER ${folder_name})
    endif()
endmacro()

# aws-c-common
rawr_target_folder(aws-c-common "deps")
rawr_target_folder(aws-c-common-assert-tests "deps")
rawr_target_folder(aws-c-common-tests "deps")

# mn
rawr_target_folder(mn "deps")
rawr_target_folder(mn-tests "tests/mn")

# libxml2
rawr_target_folder(LibXml2 "deps")
rawr_target_folder(runsuite "tests/LibXml2")
rawr_target_folder(runtest "tests/LibXml2")
rawr_target_folder(runxmlconf "tests/LibXml2")
rawr_target_folder(testapi "tests/LibXml2")
rawr_target_folder(testAutomata "tests/LibXml2")
rawr_target_folder(testC14N "tests/LibXml2")
rawr_target_folder(testchar "tests/LibXml2")
rawr_target_folder(testdict "tests/LibXml2")
rawr_target_folder(testHTML "tests/LibXml2")
rawr_target_folder(testlimits "tests/LibXml2")
rawr_target_folder(testModule "tests/LibXml2")
rawr_target_folder(testReader "tests/LibXml2")
rawr_target_folder(testrecurse "tests/LibXml2")
rawr_target_folder(testRegexp "tests/LibXml2")
rawr_target_folder(testRelax "tests/LibXml2")
rawr_target_folder(testSAX "tests/LibXml2")
rawr_target_folder(testSchemas "tests/LibXml2")
rawr_target_folder(testThreads "tests/LibXml2")
rawr_target_folder(testURI "tests/LibXml2")
rawr_target_folder(testXPath "tests/LibXml2")
rawr_target_folder(LibXml2Mod "extras/LibXml2")
rawr_target_folder(xmlcatalog "examples/LibXml2")
rawr_target_folder(xmllint "examples/LibXml2")

# opus
rawr_target_folder(opus "deps")
rawr_target_folder(test_opus_api "tests/opus")
rawr_target_folder(test_opus_decode "tests/opus")
rawr_target_folder(test_opus_encode "tests/opus")
rawr_target_folder(test_opus_padding "tests/opus")

# libsrtp
rawr_target_folder(srtp2 "deps")
rawr_target_folder(srtp_driver "examples/libsrtp")
rawr_target_folder(rtpw "examples/libsrtp")
rawr_target_folder(test_srtp "tests/libsrtp")

# strophe
rawr_target_folder(strophe "deps")
rawr_target_folder(strophe-xep "deps")
rawr_target_folder(active "examples/strophe")
rawr_target_folder(basic "examples/strophe")
rawr_target_folder(bot "examples/strophe")
rawr_target_folder(component "examples/strophe")
rawr_target_folder(register "examples/strophe")
rawr_target_folder(roster "examples/strophe")
rawr_target_folder(uuid "examples/strophe")
rawr_target_folder(strophe-xep_example "examples/strophe-xep")

# portaudio
rawr_target_folder(portaudio "deps")
rawr_target_folder(portaudio_static "extras/portaudio")
rawr_target_folder(patest_longsine "tests/portaudio")
rawr_target_folder(pa_devs "examples/portaudio")
rawr_target_folder(pa_fuzz "examples/portaudio")
rawr_target_folder(paex_ocean_shore "examples/portaudio")
rawr_target_folder(paex_pink "examples/portaudio")
rawr_target_folder(paex_read_write_wire "examples/portaudio")
rawr_target_folder(paex_record "examples/portaudio")
rawr_target_folder(paex_record_file "examples/portaudio")
rawr_target_folder(paex_saw "examples/portaudio")
rawr_target_folder(paex_sine "examples/portaudio")
rawr_target_folder(paex_write_sine "examples/portaudio")
rawr_target_folder(paex_write_sine_nonint "examples/portaudio")
rawr_target_folder(paex_sine_c++ "examples/portaudio")
if (WIN32)
	rawr_target_folder(paex_wmme_ac3 "examples/portaudio")
	rawr_target_folder(paex_wmme_surround "examples/portaudio")
endif()

# libuv
rawr_target_folder(uv_a "deps")
rawr_target_folder(uv "extras/libuv")

# libjuice
rawr_target_folder(juice "extras/juice")
rawr_target_folder(juice-static "extras/juice")
rawr_target_folder(juice-tests "extras/juice")

# re
rawr_target_folder(re "deps")

# playgrounds
rawr_target_folder(playground_juice "playgrounds")
rawr_target_folder(playground_lws "playgrounds")
rawr_target_folder(playground_pa "playgrounds")
rawr_target_folder(playground_re_sip "playgrounds")
rawr_target_folder(playground_srtp_recv "playgrounds")
rawr_target_folder(playground_srtp_send "playgrounds")
rawr_target_folder(playground_srtp_sendrecv "playgrounds")
rawr_target_folder(playground_srtp_siprecv "playgrounds")
rawr_target_folder(playground_strophe "playgrounds")
rawr_target_folder(playground_ui "playgrounds")
rawr_target_folder(playground_juice "playgrounds")
rawr_target_folder(playground_ice "playgrounds")
