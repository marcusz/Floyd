//
//  expressions.h
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 03/08/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#ifndef expressions_hpp
#define expressions_hpp



#include "quark.h"
#include <vector>
#include <string>

#include "parser_types.h"
#include "parser_value.h"


namespace floyd_parser {
	struct expression_t;
	struct statement_t;
	struct ast_t;

	struct math_operation1_expr_t;

	//////////////////////////////////////////////////		constant expression
	
	//	Has no type, uses value_t.
	

	//////////////////////////////////////////////////		math_operation1_expr_t


	struct math_operation1_expr_t {
		bool operator==(const math_operation1_expr_t& other) const;


		enum operation {
			negate = 20
		};

		const operation _operation;

		const std::shared_ptr<expression_t> _input;
	};

	//////////////////////////////////////////////////		math_operation2_expr_t


	struct math_operation2_expr_t {
		bool operator==(const math_operation2_expr_t& other) const;


		enum operation {
			add = 10,
			subtract,
			multiply,
			divide
		};

		const operation _operation;

		const std::shared_ptr<expression_t> _left;
		const std::shared_ptr<expression_t> _right;
	};


	//////////////////////////////////////////////////		function_call_expr_t


	struct function_call_expr_t {
		bool operator==(const function_call_expr_t& other) const{
			return _function_name == other._function_name && _inputs == other._inputs;
		}


		const std::string _function_name;
		const std::vector<std::shared_ptr<expression_t>> _inputs;
	};


	//////////////////////////////////////////////////		variable_read_expr_t

	/*
		Supports reading a named variable, like "int a = 10; print(a);"
	*/
	struct variable_read_expr_t {
		bool operator==(const variable_read_expr_t& other) const;


		std::shared_ptr<expression_t> _address;
	};


	//////////////////////////////////////////////////		resolve_member_expr_t

	/*
		Supports reading a named variable, like "int a = 10; print(a);"
	*/
	struct resolve_member_expr_t {
		bool operator==(const resolve_member_expr_t& other) const{
			return _member_name == other._member_name ;
		}

		const std::string _member_name;
	};


	//////////////////////////////////////////////////		lookup_element_expr_t

	/*
		Looksup using a key. They key can be a sub-expression.
	*/
	struct lookup_element_expr_t {
		bool operator==(const lookup_element_expr_t& other) const;

		std::shared_ptr<expression_t> _lookup_key;
	};





//////////////////////////////////////////////////		expression_t


	struct expression_t {
		public: bool check_invariant() const{
			return true;
		}

		expression_t(){
		}

		expression_t(const math_operation1_expr_t& value) :
			_math_operation1_expr(std::make_shared<math_operation1_expr_t>(value))
		{
			QUARK_ASSERT(check_invariant());
		}
		expression_t(const math_operation2_expr_t& value) :
			_math_operation2_expr(std::make_shared<math_operation2_expr_t>(value))
		{
			QUARK_ASSERT(check_invariant());
		}

		expression_t(const function_call_expr_t& value) :
			_call_function_expr(std::make_shared<function_call_expr_t>(value))
		{
			QUARK_ASSERT(check_invariant());
		}

		expression_t(const resolve_member_expr_t& value) :
			_resolve_member_expr(std::make_shared<resolve_member_expr_t>(value))
		{
			QUARK_ASSERT(check_invariant());
		}

		expression_t(const lookup_element_expr_t& value) :
			_lookup_element_expr(std::make_shared<lookup_element_expr_t>(value))
		{
			QUARK_ASSERT(check_invariant());
		}

		bool operator==(const expression_t& other) const;


		//////////////////////////		STATE

		/*
			Only one of there are used at any time.
		*/
		std::shared_ptr<value_t> _constant;
		std::shared_ptr<math_operation1_expr_t> _math_operation1_expr;
		std::shared_ptr<math_operation2_expr_t> _math_operation2_expr;
		std::shared_ptr<function_call_expr_t> _call_function_expr;
		std::shared_ptr<variable_read_expr_t> _variable_read_expr;
		std::shared_ptr<resolve_member_expr_t> _resolve_member_expr;
		std::shared_ptr<lookup_element_expr_t> _lookup_element_expr;
	};


	//////////////////////////////////////////////////		inlines


	inline bool math_operation2_expr_t::operator==(const math_operation2_expr_t& other) const {
		return _operation == other._operation && *_left == *other._left && *_right == *other._right;
	}
	inline bool math_operation1_expr_t::operator==(const math_operation1_expr_t& other) const {
		return _operation == other._operation && *_input == *other._input;
	}

	inline bool variable_read_expr_t::operator==(const variable_read_expr_t& other) const{
		return *_address == *other._address ;
	}

	inline bool lookup_element_expr_t::operator==(const lookup_element_expr_t& other) const{
		return *_lookup_key == *other._lookup_key ;
	}



	//////////////////////////////////////////////////		make specific expressions

	expression_t make_constant(const value_t& value);
	expression_t make_constant(const std::string& s);
	expression_t make_constant(const int i);
	expression_t make_constant(const float f);


	inline expression_t make_math_operation2_expr(const math_operation2_expr_t& value){
		return expression_t(value);
	}

	inline expression_t make_math_operation2_expr(math_operation2_expr_t::operation op, const expression_t& left, const expression_t& right){
		return expression_t(math_operation2_expr_t{op, std::make_shared<expression_t>(left), std::make_shared<expression_t>(right)});
	}

	expression_t make_math_operation1(math_operation1_expr_t::operation op, const expression_t& input);

	expression_t make_variable_read(const expression_t& address_expression);
	expression_t make_variable_read_variable(const std::string& name);




	//////////////////////////////////////////////////		trace()


	void trace(const math_operation1_expr_t& e);
	void trace(const math_operation2_expr_t& e);
	void trace(const function_call_expr_t& e);
	void trace(const variable_read_expr_t& e);
	void trace(const resolve_member_expr_t& e);
	void trace(const lookup_element_expr_t& e);
	void trace(const expression_t& e);

}


#endif /* expressions_hpp */
