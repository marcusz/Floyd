//
//  floyd_llvm.hpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-08-19.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

//	High-level functions for command line tool (and other clients potentially).
//	Machine-only features: no "UI", terminal messages etc. allowed here.


#ifndef floyd_llvm_hpp
#define floyd_llvm_hpp


#include <string>
#include <vector>

#include "compiler_helpers.h"
#include "compiler_basics.h"

namespace floyd {

struct config_t;
struct bench_t;
struct test_t;
struct run_output_t;
struct benchmark_result2_t;



//	Compiles and runs the program. Returns results.
run_output_t run_program_helper(
	const std::string& program_source,
	const std::string& file,
	compilation_unit_mode mode,
	const compiler_settings_t& settings,
	const std::vector<std::string>& main_args,
	bool run_tests
);


//////////////////////////////////////////		BENCHMARK


std::vector<bench_t> filter_benchmarks(const std::vector<bench_t>& b, const std::vector<std::string>& run_tests);

std::vector<bench_t> collect_benchmarks_source(
	const std::string& program_source,
	const std::string& file,
	compilation_unit_mode mode,
	const compiler_settings_t& settings
);

//	Tests empty = ALL tests.
std::vector<benchmark_result2_t> run_benchmarks_source(
	const std::string& program_source,
	const std::string& source_path,
	const compiler_settings_t& compiler_settings,
	const std::vector<std::string>& tests
);


//////////////////////////////////////////		TESTS


std::vector<test_t> filter_tests(const std::vector<test_t>& b, const std::vector<std::string>& run_tests);

class test_progress {
	public: virtual ~test_progress(){};

	public: virtual void test_progress__on_start(int index) = 0;
	public: virtual void test_progress__on_end(int index, const std::string& result) = 0;
};


std::vector<test_t> collect_tests_source(
	const std::string& program_source,
	const std::string& file,
	compilation_unit_mode mode,
	const compiler_settings_t& settings
);

//	Tests empty = ALL tests.
std::vector<std::string> run_tests_source(
	const std::string& program_source,
	const std::string& source_path,
	const compiler_settings_t& compiler_settings,
	const std::vector<std::string>& tests
);


}	// floyd

#endif /* floyd_llvm_hpp */
