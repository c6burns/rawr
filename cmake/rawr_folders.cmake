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

# opus
rawr_target_folder(opus "deps")
rawr_target_folder(test_opus_api "tests/opus")
rawr_target_folder(test_opus_decode "tests/opus")
rawr_target_folder(test_opus_encode "tests/opus")
rawr_target_folder(test_opus_padding "tests/opus")

# portaudio
rawr_target_folder(portaudio_static "deps")
rawr_target_folder(portaudio "extras/portaudio")
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

# re
rawr_target_folder(re "deps")

# playgrounds
rawr_target_folder(playground_lws "playgrounds")
rawr_target_folder(playground_pa "playgrounds")
rawr_target_folder(playground_pa_callbacks "playgrounds")
rawr_target_folder(playground_re_sip "playgrounds")
rawr_target_folder(playground_ui "playgrounds")
rawr_target_folder(playground_ice "playgrounds")
rawr_target_folder(playground_stun "playgrounds")
rawr_target_folder(playground_call "playgrounds")
