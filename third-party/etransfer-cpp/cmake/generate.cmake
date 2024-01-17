include(ExternalProject)

set(ANTLR4_GENERATED_SRC_DIR ${CMAKE_BINARY_DIR}/generated)

#generate ob code from .g4 file
set(GRAMMAR_OB_LEXER ${PROJECT_SOURCE_DIR}/grammar/oceanbase/OBLexer.g4)
set(GRAMMAR_OB_PARSER ${PROJECT_SOURCE_DIR}/grammar/oceanbase/OBParser.g4)

execute_process(
        COMMAND
        "${Java_JAVA_EXECUTABLE}" -jar "${ANTLR4_JAR_DOWNLOAD_DIR}" -Werror -Dlanguage=Cpp -o "${ANTLR4_GENERATED_SRC_DIR}/oceanbase" -package "oceanbase" "${GRAMMAR_OB_LEXER}" "${GRAMMAR_OB_PARSER}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

file(GLOB_RECURSE generated_ob_srcs CONFIGURE_DEPENDS ${ANTLR4_GENERATED_SRC_DIR}/oceanbase/*.cpp)
add_library(obsqlparser STATIC ${generated_ob_srcs})
target_link_libraries(obsqlparser PUBLIC antlr4)