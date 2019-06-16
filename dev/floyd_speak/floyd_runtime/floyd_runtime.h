//
//  floyd_runtime.hpp
//  floyd_speak
//
//  Created by Marcus Zetterquist on 2019-02-17.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#ifndef floyd_runtime_hpp
#define floyd_runtime_hpp


#include <string>
#include "ast_typeid.h"
#include "compiler_basics.h"

struct json_t;

namespace floyd {

struct value_t;


//////////////////////////////////////		runtime_handler_i

/*
	This is a callback from the interpreter (really the host functions run from the interpeter)
	that allows the interpreter to indirectly control the outside runtime that hosts the interpreter.
	FUTURE: Needs neater solution than this.
*/
struct runtime_handler_i {
	virtual ~runtime_handler_i(){};
	virtual void on_send(const std::string& process_id, const json_t& message) = 0;
};


inline typeid_t get_main_signature_arg_impure(){
	return typeid_t::make_function(typeid_t::make_int(), { typeid_t::make_vector(typeid_t::make_string()) }, epure::impure);
}

inline typeid_t get_main_signature_no_arg_impure(){
 	return typeid_t::make_function(typeid_t::make_int(), { }, epure::impure);
}


inline typeid_t get_main_signature_arg_pure(){
	return typeid_t::make_function(typeid_t::make_int(), { typeid_t::make_vector(typeid_t::make_string()) }, epure::pure);
}

inline typeid_t get_main_signature_no_arg_pure(){
	return typeid_t::make_function(typeid_t::make_int(), { }, epure::pure);
}

//	T x_init() impure
typeid_t make_process_init_type(const typeid_t& t);

//	T x(T state, json_value message) impure
typeid_t make_process_message_handler_type(const typeid_t& t);





value_t unflatten_json_to_specific_type(const json_t& v, const typeid_t& target_type);





//////////////////////////////////////		HOST FUNCTIONS




struct host_function_signature_t {
	std::string name;
	function_id_t _function_id;
	floyd::typeid_t _function_type;
};

host_function_signature_t make_assert_signature();
host_function_signature_t make_to_string_signature();
host_function_signature_t make_to_pretty_string_signature();
host_function_signature_t make_typeof_signature();
host_function_signature_t make_update_signature();
host_function_signature_t make_size_signature();
host_function_signature_t make_find_signature();
host_function_signature_t make_exists_signature();
host_function_signature_t make_erase_signature();
host_function_signature_t make_push_back_signature();
host_function_signature_t make_subset_signature();
host_function_signature_t make_replace_signature();

host_function_signature_t make_script_to_jsonvalue_signature();
host_function_signature_t make_jsonvalue_to_script_signature();
host_function_signature_t make_value_to_jsonvalue_signature();
host_function_signature_t make_jsonvalue_to_value_signature();

host_function_signature_t make_get_json_type_signature();

host_function_signature_t make_calc_string_sha1_signature();
host_function_signature_t make_calc_binary_sha1_signature();

host_function_signature_t make_map_signature();
host_function_signature_t make_map_string_signature();
host_function_signature_t make_filter_signature();
host_function_signature_t make_reduce_signature();
host_function_signature_t make_supermap_signature();


host_function_signature_t make_print_signature();
host_function_signature_t make_send_signature();
host_function_signature_t make_get_time_of_day_signature();

host_function_signature_t make_read_text_file_signature();
host_function_signature_t make_write_text_file_signature();

host_function_signature_t make_get_fsentries_shallow_signature();
host_function_signature_t make_get_fsentries_deep_signature();
host_function_signature_t make_get_fsentry_info_signature();
host_function_signature_t make_get_fs_environment_signature();
host_function_signature_t make_does_fsentry_exist_signature();
host_function_signature_t make_create_directory_branch_signature();
host_function_signature_t make_delete_fsentry_deep_signature();
host_function_signature_t make_rename_fsentry_signature();




std::map<std::string, host_function_signature_t> get_host_function_signatures();



}	//	floyd

#endif /* floyd_runtime_hpp */
