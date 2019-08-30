//
//  parser_ast.cpp
//  Floyd
//
//  Created by Marcus Zetterquist on 10/08/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#include "ast.h"


#include "statement.h"
#include "utils.h"
#include "json_support.h"
#include "parser_primitives.h"


namespace floyd {



//////////////////////////////////////////////////		general_purpose_ast_t



json_t gp_ast_to_json(const general_purpose_ast_t& ast){
	QUARK_ASSERT(ast.check_invariant());

	std::vector<json_t> fds;
	for(const auto& e: ast._function_defs){
		const auto fd = function_def_to_ast_json(e);
		fds.push_back(fd);
	}

	const auto function_defs_json = json_t::make_array(fds);
	return json_t::make_object(
		{
			{ "globals", body_to_json(ast._globals) },
			{ "function_defs", function_defs_json }
		}
	);

	//??? add software-system and containerdef.
}

general_purpose_ast_t json_to_gp_ast(const json_t& json){
	const auto globals0 = json.get_object_element("globals");
	const auto function_defs = json.get_object_element("function_defs");

	body_t globals1 = json_to_body(globals0);

	std::vector<floyd::function_definition_t> function_defs1;
	for(const auto& f: function_defs.get_array()){
		const auto f1 = json_to_function_def(f);
		function_defs1.push_back(f1);
	}

	return general_purpose_ast_t {
		globals1,
		function_defs1,
		{},
		software_system_t{},
		container_t{}
	};
}



} //	floyd
