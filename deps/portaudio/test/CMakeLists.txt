# Test projects
# Use the macro to add test projects

MACRO(PA_ADD_TEST appl_name)
   ADD_EXECUTABLE(${appl_name} "${appl_name}.c")
   TARGET_LINK_LIBRARIES(${appl_name} portaudio_static)
   SET_TARGET_PROPERTIES(${appl_name}
      PROPERTIES
      FOLDER "Test"
   )
ENDMACRO(PA_ADD_TEST)

PA_ADD_TEST(patest_longsine)
