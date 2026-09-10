add_executable(compiler
    src/main.cpp
 "src/lexer.cpp" "src/lexer.h" "src/visitor.h" "src/parser.cpp" "src/parser.h" 
 "src/visitor.cpp" "src/astprinter.h" "src/irgenerator.h" "src/irgenerator.cpp"
 "src/regalloc.h" "src/regalloc.cpp")

target_include_directories(compiler PRIVATE src)

target_link_libraries(compiler PRIVATE riscv_shared)

target_compile_options(compiler PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)