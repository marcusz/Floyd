//
//  floyd_llvm.cpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-08-19.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "floyd_llvm.h"

#include "floyd_llvm_runtime.h"
#include "value_backend.h"
#include "floyd_llvm_codegen.h"
#include "semantic_ast.h"
#include "compiler_helpers.h"
#include "floyd_corelib.h"


namespace floyd {


run_output_t run_program_helper(
	const std::string& program_source,
	const std::string& file,
	compilation_unit_mode mode,
	const compiler_settings_t& settings,
	const std::vector<std::string>& main_args,
	bool run_tests
){
	QUARK_ASSERT(settings.check_invariant());

	const auto cu = floyd::make_compilation_unit(program_source, file, mode);
	const auto sem_ast = compile_to_sematic_ast__errors(cu);

	llvm_instance_t instance;
	auto program = generate_llvm_ir_program(instance, sem_ast, file, settings);
	auto ee = init_llvm_jit(*program);

	if(run_tests){
//???		run_tests(ee);
	}

	const auto result = run_program(*ee, main_args);
	return result;
}


//////////////////////////////////////////		BENCHMARK


std::vector<bench_t> filter_benchmarks(const std::vector<bench_t>& b, const std::vector<std::string>& run_tests){
	std::vector<bench_t> filtered;
	for(const auto& wanted_test: run_tests){
		const auto it = std::find_if(b.begin(), b.end(), [&] (const bench_t& b2) { return b2.benchmark_id.test == wanted_test; } );
		if(it != b.end()){
			filtered.push_back(*it);
		}
	}

	return filtered;
}

std::vector<bench_t> collect_benchmarks_source(const std::string& program_source, const std::string& file, compilation_unit_mode mode, const compiler_settings_t& settings){
	QUARK_ASSERT(settings.check_invariant());

	const auto cu = floyd::make_compilation_unit(program_source, file, mode);
	const auto sem_ast = compile_to_sematic_ast__errors(cu);

	llvm_instance_t instance;
	auto program = generate_llvm_ir_program(instance, sem_ast, file, settings);
	auto ee = init_llvm_jit(*program);

	std::vector<bench_t> b = collect_benchmarks(*ee);
	return b;
//	const auto result = mapf<benchmark_id_t>(b, [](auto& e){ return e.benchmark_id; });
}

QUARK_TEST("", "collect_benchmarks_source()", "", ""){
//	g_trace_on = true;
	const auto program_source =
	R"(

		benchmark-def "pack_png 1" {
			return [ benchmark_result_t(200, json("0 elements")) ]
		}

		benchmark-def "pack_png 2" {
			return [ benchmark_result_t(200, json("0 elements")) ]
		}

		benchmark-def "pack_png no compress" {
			return [ benchmark_result_t(300, json("bytes/s")) ]
		}

	)";

	const auto result = collect_benchmarks_source(program_source, "module1", compilation_unit_mode::k_include_core_lib, make_default_compiler_settings());
//	const auto result = do_user_benchmarks_list(program_source, "module1");


	std::stringstream ss;
	ss << "Benchmarks registry:" << std::endl;
	for(const auto& e: result){
		ss << e.benchmark_id.test << std::endl;
	}

	std::cout << ss.str();
}

QUARK_TEST("", "collect_benchmarks_source()", "", ""){
//	g_trace_on = true;
	const auto program_source =
	R"(

		benchmark-def "abc" {
			return [ benchmark_result_t(200, json("0 elements")) ]
		}

		benchmark-def "def" {
			return [ benchmark_result_t(200, json("0 elements")) ]
		}

		benchmark-def "g" {
			return [ benchmark_result_t(300, json("bytes/s")) ]
		}

	)";

	const auto result = collect_benchmarks_source(program_source, "module1", compilation_unit_mode::k_include_core_lib, make_default_compiler_settings());

	std::stringstream ss;
	ss << "Benchmarks registry:" << std::endl;
	for(const auto& e: result){
		ss << e.benchmark_id.test << std::endl;
	}

	std::cout << ss.str();

//	ut_verify(QUARK_POS, result, "\"\": \"abc\"\n\"\": \"def\"\n\"\": \"g\"\n");
	ut_verify_string(QUARK_POS, ss.str(), "Benchmarks registry:\n" "abc\n" "def\n" "g\n");
}

QUARK_TEST("", "collect_benchmarks_source()", "", ""){
//	g_trace_on = true;
	const auto program_source =
	R"(

		benchmark-def "abc" {
			return [ benchmark_result_t(200, json("0 elements")) ]
		}

		benchmark-def "def" {
			return [
				benchmark_result_t(1, json("first")),
				benchmark_result_t(2, json("second")),
				benchmark_result_t(3, json("third"))
			]
		}

		benchmark-def "g" {
			return [ benchmark_result_t(300, json("bytes/s")) ]
		}

	)";

	const auto result = collect_benchmarks_source(program_source, "mymodule", compilation_unit_mode::k_include_core_lib, make_default_compiler_settings());

	QUARK_VERIFY(result.size() == 3);
	QUARK_VERIFY(result[0] == (bench_t{ benchmark_id_t{ "", "abc" }, encode_floyd_func_link_name("benchmark__abc") }));
	QUARK_VERIFY(result[1] == (bench_t{ benchmark_id_t{ "", "def" }, encode_floyd_func_link_name("benchmark__def") }));
	QUARK_VERIFY(result[2] == (bench_t{ benchmark_id_t{ "", "g" }, encode_floyd_func_link_name("benchmark__g") }));
}


static std::vector<benchmark_result2_t> run_benchmarks_source(const std::string& program_source, const std::string& file, compilation_unit_mode mode, const compiler_settings_t& settings, const std::vector<std::string>& tests){
	QUARK_ASSERT(settings.check_invariant());

	const auto cu = floyd::make_compilation_unit(program_source, file, mode);
	const auto sem_ast = compile_to_sematic_ast__errors(cu);

	llvm_instance_t instance;
	auto program = generate_llvm_ir_program(instance, sem_ast, file, settings);
	auto ee = init_llvm_jit(*program);

	const auto b = collect_benchmarks(*ee);
	const auto b2 = filter_benchmarks(b, tests);
	if(b2.size() < tests.size()){
		QUARK_TRACE("Some specified tests were not found");
	}

	std::vector<benchmark_result2_t> b3 = run_benchmarks(*ee, b2);

	return b3;
}

QUARK_TEST("", "run_benchmarks_source()", "", ""){
	run_benchmarks_source(
		R"(

			benchmark-def "AAA" {
				return [ benchmark_result_t(200, json("0 eleements")) ]
			}
			benchmark-def "BBB" {
				return [ benchmark_result_t(300, json("3 monkeys")) ]
			}
		)",
		"myfile.floyd",
		compilation_unit_mode::k_no_core_lib,
		make_default_compiler_settings(),
		{ }
	);
	QUARK_VERIFY(true);
}

QUARK_TEST("", "run_benchmarks_source()", "", ""){
//	g_trace_on = true;
	const auto program_source =
	R"(

		benchmark-def "ABC" {
			return [ benchmark_result_t(200, json("0 elements")) ]
		}

	)";

	const auto result = run_benchmarks_source(program_source, "module1", compilation_unit_mode::k_include_core_lib, make_default_compiler_settings(), { "ABC" });

	QUARK_VERIFY(result.size() == 1);
	QUARK_VERIFY(result[0] == (benchmark_result2_t { benchmark_id_t{ "", "ABC" }, benchmark_result_t { 200, json_t("0 elements") } }));
}

QUARK_TEST("", "run_benchmarks_source()", "", ""){
//	g_trace_on = true;
	const auto program_source =
	R"(

		benchmark-def "abc" {
			return [ benchmark_result_t(200, json("0 elements")) ]
		}

		benchmark-def "def" {
			return [
				benchmark_result_t(1, json("first")),
				benchmark_result_t(2, json("second")),
				benchmark_result_t(3, json("third"))
			]
		}

		benchmark-def "g" {
			return [ benchmark_result_t(300, json("bytes/s")) ]
		}

	)";

	const auto result = run_benchmarks_source(program_source, "", compilation_unit_mode::k_include_core_lib, make_default_compiler_settings(), { "abc", "def", "g" });

	QUARK_VERIFY(result.size() == 5);
	QUARK_VERIFY(result[0] == (benchmark_result2_t { benchmark_id_t{ "", "abc" }, benchmark_result_t { 200, json_t("0 elements") } }));
	QUARK_VERIFY(result[1] == (benchmark_result2_t { benchmark_id_t{ "", "def" }, benchmark_result_t { 1, json_t("first") } }));
	QUARK_VERIFY(result[2] == (benchmark_result2_t { benchmark_id_t{ "", "def" }, benchmark_result_t { 2, json_t("second") } }));
	QUARK_VERIFY(result[3] == (benchmark_result2_t { benchmark_id_t{ "", "def" }, benchmark_result_t { 3, json_t("third") } }));
	QUARK_VERIFY(result[4] == (benchmark_result2_t { benchmark_id_t{ "", "g" }, benchmark_result_t { 300, json_t("bytes/s") } }));
}

//??? Only compile once!
std::string run_all_benchmarks_source(
	const std::string& program_source,
	const std::string& source_path,
	const compiler_settings_t& compiler_settings
){
	const auto b = collect_benchmarks_source(program_source, source_path, compilation_unit_mode::k_include_core_lib, compiler_settings);
	const auto b2 = mapf<std::string>(b, [](const bench_t& e){ return e.benchmark_id.test; });
	const auto results = run_benchmarks_source(program_source, source_path, compilation_unit_mode::k_include_core_lib, compiler_settings, b2);

	return make_benchmark_report(results);
}

std::string run_specific_benchmarks_source(
	const std::string& program_source,
	const std::string& source_path,
	const compiler_settings_t& compiler_settings,
	const std::vector<std::string>& tests
){
	const auto b = collect_benchmarks_source(program_source, source_path, compilation_unit_mode::k_include_core_lib, compiler_settings);
	const auto c = filter_benchmarks(b, tests);
	const auto b2 = mapf<std::string>(c, [](const bench_t& e){ return e.benchmark_id.test; });
	const auto results = run_benchmarks_source(program_source, source_path, compilation_unit_mode::k_include_core_lib, compiler_settings, b2);

	return make_benchmark_report(results);
}



//////////////////////////////////////////		TESTS




std::vector<test_t> filter_tests(const std::vector<test_t>& b, const std::vector<std::string>& run_tests){
	std::vector<test_t> filtered;
	for(const auto& wanted_test: run_tests){
		const auto it = std::find_if(b.begin(), b.end(), [&] (const test_t& b2) { return b2.test_id.test == wanted_test; } );
		if(it != b.end()){
			filtered.push_back(*it);
		}
	}

	return filtered;
}

static std::vector<std::string> run_tests_source(const std::string& program_source, const std::string& file, compilation_unit_mode mode, const compiler_settings_t& settings, const std::vector<std::string>& tests){
	QUARK_ASSERT(settings.check_invariant());

	const auto cu = floyd::make_compilation_unit(program_source, file, mode);
	const auto sem_ast = compile_to_sematic_ast__errors(cu);

	llvm_instance_t instance;
	auto program = generate_llvm_ir_program(instance, sem_ast, file, settings);
	auto ee = init_llvm_jit(*program);

	const auto b = collect_tests(*ee);
	const auto b2 = filter_tests(b, tests);
	if(b2.size() < tests.size()){
		QUARK_TRACE("Some specified tests were not found");
	}

	std::vector<std::string> b3 = run_tests(*ee, b2);

	return b3;
}

std::vector<test_t> collect_tests_source(
	const std::string& program_source,
	const std::string& file,
	compilation_unit_mode mode,
	const compiler_settings_t& settings
){
	QUARK_ASSERT(settings.check_invariant());

	const auto cu = floyd::make_compilation_unit(program_source, file, mode);
	const auto sem_ast = compile_to_sematic_ast__errors(cu);

	llvm_instance_t instance;
	auto program = generate_llvm_ir_program(instance, sem_ast, file, settings);
	auto ee = init_llvm_jit(*program);

	std::vector<test_t> b = collect_tests(*ee);
	return b;
//	const auto result = mapf<test_id_t>(b, [](auto& e){ return e.test_id; });
}

std::string run_all_tests_source(
	const std::string& program_source,
	const std::string& source_path,
	const compiler_settings_t& compiler_settings
){
	const auto b = collect_tests_source(program_source, source_path, compilation_unit_mode::k_include_core_lib, compiler_settings);
	const auto b2 = mapf<std::string>(b, [](const test_t& e){ return e.test_id.test; });
	const auto results = run_tests_source(program_source, source_path, compilation_unit_mode::k_include_core_lib, compiler_settings, b2);

	return "??? TODO";
}

std::string run_specific_tests_source(
	const std::string& program_source,
	const std::string& source_path,
	const compiler_settings_t& compiler_settings,
	const std::vector<std::string>& tests
){
	const auto b = collect_tests_source(program_source, source_path, compilation_unit_mode::k_include_core_lib, compiler_settings);
	const auto c = filter_tests(b, tests);
	const auto b2 = mapf<std::string>(c, [](const test_t& e){ return e.test_id.test; });
	const auto results = run_tests_source(program_source, source_path, compilation_unit_mode::k_include_core_lib, compiler_settings, b2);

	return "??? TODO";
}



}	//	namespace floyd




////////////////////////////////		TESTS



QUARK_TEST("", "From source: Check that floyd_runtime_init() runs and sets 'result' global", "", ""){
	const auto cu = floyd::make_compilation_unit_nolib("let int result = 1 + 2 + 3", "myfile.floyd");
	const auto sem_ast = compile_to_sematic_ast__errors(cu);

	floyd::llvm_instance_t instance;
	auto program = generate_llvm_ir_program(instance, sem_ast, "myfile.floyd", floyd::make_default_compiler_settings());
	auto ee = init_llvm_jit(*program);

	const auto result = *static_cast<uint64_t*>(floyd::get_global_ptr(*ee, "result"));
	QUARK_ASSERT(result == 6);

//	QUARK_TRACE_SS("result = " << floyd::print_program(*program));
}

//	BROKEN!
QUARK_TEST("", "From JSON: Simple function call, call print() from floyd_runtime_init()", "", ""){
	const auto cu = floyd::make_compilation_unit_nolib("print(5)", "myfile.floyd");
	const auto sem_ast = compile_to_sematic_ast__errors(cu);

	floyd::llvm_instance_t instance;
	auto program = generate_llvm_ir_program(instance, sem_ast, "myfile.floyd", floyd::make_default_compiler_settings());
	auto ee = init_llvm_jit(*program);
	QUARK_ASSERT(ee->_print_output == std::vector<std::string>{"5"});
}





