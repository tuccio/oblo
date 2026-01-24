#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/file.hpp>
#include <oblo/core/platform/process.hpp>
#include <oblo/core/platform/shared_library.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/script/compiler/bytecode_generator.hpp>
#include <oblo/script/compiler/cpp_compiler.hpp>
#include <oblo/script/compiler/cpp_generator.hpp>
#include <oblo/script/interpreter.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace oblo
{
    namespace
    {
        abstract_syntax_tree make_add_sub_f32_constants_ast()
        {
            abstract_syntax_tree ast;
            ast.init();

            const auto root = ast.get_root();

            ast.add_node(root, ast_type_declaration{.name = "f32", .size = sizeof(f32)});
            const auto hFunc = ast.add_node(root, ast_function_declaration{.name = "add_sub", .returnType = "f32"});
            const auto hBody = ast.add_node(hFunc, ast_function_body{});
            const auto hReturn = ast.add_node(hBody, ast_return_statement{});

            const auto hAdd = ast.add_node(hReturn, ast_binary_operator{.op = ast_binary_operator_kind::add_f32});
            ast.add_node(hAdd, ast_f32_constant{40.f});

            const auto hSub = ast.add_node(hAdd, ast_binary_operator{.op = ast_binary_operator_kind::sub_f32});
            ast.add_node(hSub, ast_f32_constant{5.f});
            ast.add_node(hSub, ast_f32_constant{3.f});

            return ast;
        }
    }

    TEST(bytecode_generator, add_sub_f32_constants)
    {
        const abstract_syntax_tree ast = make_add_sub_f32_constants_ast();

        bytecode_generator gen;
        const auto m = gen.generate_module(ast);
        ASSERT_TRUE(m);

        interpreter rt;
        rt.init(1u << 8);

        rt.load_module(*m);
        const h32 hFuncInstance = rt.find_function("add_sub"_hsv);
        ASSERT_TRUE(hFuncInstance);

        ASSERT_TRUE(rt.call_function(hFuncInstance));

        // We should have the result at the top
        const expected<f32, interpreter_error> r = rt.read_f32(0);
        ASSERT_TRUE(r);

        ASSERT_EQ(rt.used_stack_size(), sizeof(f32));
        ASSERT_FLOAT_EQ(*r, 42.f);
    }

    namespace
    {
        void dump_to_stdout(const platform::file& rPipe)
        {
            if (rPipe.is_open())
            {
                char buf[1024];

                while (true)
                {
                    const usize readBytes = rPipe.read(buf, sizeof(buf)).value_or(0);

                    std::fwrite(buf, 1, readBytes, stdout);

                    if (readBytes != sizeof(buf))
                    {
                        break;
                    }
                }
            }
        }

        void compile_script(string_view code, cstring_view name, platform::shared_library& lib)
        {
            const auto compiler = cpp_compiler::find();
            ASSERT_TRUE(compiler);

            string_builder src;
            src.format("{}.cpp", name);

            ASSERT_TRUE(filesystem::write_file(src, as_bytes(std::span{code.data(), code.size()}), {}));

            string_builder dst;
            filesystem::current_path(dst);
            dst.append_path_separator();
            dst.format("{}.sharedlib", name);

            dynamic_array<string> compilerArgs;

            ASSERT_TRUE(compiler->make_shared_library_command_arguments(compilerArgs,
                src.view(),
                dst.view(),
                {
                    .optimizations = cpp_compiler::options::optimization_level::none,
                }));

            platform::process compile;

            dynamic_array<cstring_view> argsArray;
            argsArray.reserve(compilerArgs.size());

            for (const auto& s : compilerArgs)
            {
                argsArray.emplace_back(s);
            }

            platform::file rPipe, wPipe;

            ASSERT_TRUE(platform::file::create_pipe(rPipe, wPipe, 32 << 10u));

            ASSERT_TRUE(compile.start({
                .path = compiler->get_path(),
                .arguments = argsArray,
                .outputStream = &wPipe,
                .errorStream = &wPipe,
            }));

            ASSERT_TRUE(compile.wait());

            const i64 exitCode = compile.get_exit_code().value_or(-1);

            if (exitCode != 0)
            {
                dump_to_stdout(rPipe);
            }

            ASSERT_EQ(exitCode, 0);

            ASSERT_TRUE(lib.open(dst));
        }
    }

    TEST(cpp_generator, add_sub_f32_constants)
    {
        const abstract_syntax_tree ast = make_add_sub_f32_constants_ast();

        cpp_generator gen;
        const auto code = gen.generate_code(ast);
        ASSERT_TRUE(code);

        platform::shared_library lib;
        compile_script(code->view(), "add_sub_f32_constants", lib);

        const auto addFn = reinterpret_cast<f32 (*)()>(lib.symbol("add_sub"));
        ASSERT_TRUE(addFn);

        const f32 r = addFn();
        ASSERT_EQ(r, 42.f);
    }

    namespace
    {
        abstract_syntax_tree make_call_sin_function_ast()
        {
            abstract_syntax_tree ast;
            ast.init();

            const auto root = ast.get_root();

            ast.add_node(root, ast_type_declaration{.name = "f32", .size = sizeof(f32)});
            const auto hSinFunc = ast.add_node(root, ast_function_declaration{.name = "sin", .returnType = "f32"});
            ast.add_node(hSinFunc, ast_function_parameter{.name = "x", .type = "f32"});

            const auto hCallSinFunc =
                ast.add_node(root, ast_function_declaration{.name = "call_sin", .returnType = "f32"});

            const auto hBody = ast.add_node(hCallSinFunc, ast_function_body{});
            const auto hReturn = ast.add_node(hBody, ast_return_statement{});

            const auto hDoCallSin = ast.add_node(hReturn, ast_function_call{.name = "sin"});
            const auto hArg = ast.add_node(hDoCallSin, ast_function_argument{});
            ast.add_node(hArg, ast_f32_constant{pi / 4.f});

            return ast;
        }
    }

    TEST(cpp_generator, call_sin_function)
    {
        const abstract_syntax_tree ast = make_call_sin_function_ast();

        cpp_generator gen;
        const auto code = gen.generate_code(ast);
        ASSERT_TRUE(code);

        platform::shared_library lib;
        compile_script(code->view(), "call_sin_function", lib);

        using loader_fn = void* (*) (const char*);

        static constexpr intptr context_value = 42;

        const auto setContext = reinterpret_cast<void (*)(intptr)>(lib.symbol("oblo_set_global_context"));
        ASSERT_TRUE(setContext);

        setContext(context_value);

        const auto loadSymbols = reinterpret_cast<i32 (*)(loader_fn)>(lib.symbol("oblo_load_symbols"));
        constexpr auto loader = [](const char* name) -> void*

        {
            if (name == string_view{"sin"})
            {
                constexpr auto sin = [](const intptr context, f32 v) -> f32
                {
                    if (context != context_value)
                    {
                        return std::numeric_limits<f32>::quiet_NaN();
                    }

                    return std::sin(v);
                };

                return reinterpret_cast<void*>(+sin);
            }

            return nullptr;
        };

        ASSERT_TRUE(loadSymbols);
        ASSERT_TRUE(loadSymbols(loader));

        const auto callSin = reinterpret_cast<f32 (*)(f32)>(lib.symbol("call_sin"));

        const f32 r = callSin(pi / 4);
        ASSERT_NEAR(r, std::sin(pi / 4), 1e-6);
    }
}