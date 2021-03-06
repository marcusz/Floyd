//
//  format_table.cpp
//  floyd
//
//  Created by Marcus Zetterquist on 2019-08-13.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "format_table.h"


#include "quark.h"


std::string generate_line(const line_t& line, const std::vector<column_t>& columns){
	QUARK_ASSERT(columns.size() == line.columns.size());

	const auto bar_char = line.bar_char == 0x00 ? std::string("") : std::string(1, line.bar_char);

	std::string acc = bar_char;
	for(int c = 0 ; c < line.columns.size() ; c++){
		const auto s0 = line.columns[c];
		const auto s = std::string(columns[c].margin, line.pad_char) + s0 + std::string(columns[c].margin, line.pad_char);
		const auto wanted_width = columns[c].width + 1;

		const auto pad_count = wanted_width - size(s);
		const auto pad_string = std::string(pad_count, line.pad_char);

		const auto s2 = columns[c].align <= 0 ? s + pad_string : pad_string + s;

		acc = acc + s2 + bar_char;
	}
	return acc;
}

std::vector<column_t> fit_column(const std::vector<column_t>& start, const line_t& row){
	std::vector<column_t> acc = start;
	QUARK_ASSERT(row.columns.size() == acc.size());

	for(int column = 0 ; column < acc.size() ; column++){
		const auto acc_width = acc[column].width;
		const auto table_width = (int)row.columns[column].size() + start[column].margin * 2;
		acc[column].width = std::max(acc_width, table_width);
	}
	return acc;
}

std::vector<column_t> fit_columns(const std::vector<column_t>& start, const std::vector<line_t>& table){
	std::vector<column_t> acc = start;
	for(const auto& row: table){
		acc = fit_column(acc, row);
	}
	return acc;
}

std::vector<std::string> generate_table(const std::vector<line_t>& table, const std::vector<column_t>& columns){
	std::vector<std::string> report;
	for(const auto& line: table){
		const auto& line2 = generate_line(line, columns);
		report.push_back(line2);
	}
	return report;
}






std::string generate_table_type1(const std::vector<std::string>& headings, const std::vector<std::vector<std::string>>& matrix){
	const auto column_count = headings.size();
	const auto splitter = line_t( std::vector<std::string>(column_count, ""), '-', '|');

	std::vector<line_t> table = {
		line_t( headings, ' ', '|'),
		splitter
	};
	for(const auto& line: matrix){
		QUARK_ASSERT(line.size() == column_count);

		const auto line2 = line_t { line, ' ', '|' };
		table.push_back(line2);
	}
	table.push_back(splitter);

	const auto columns0 = std::vector<column_t>(column_count, column_t{ 0, -1, 0 });
	const auto columns = fit_columns(columns0, table);
	const auto r = generate_table(table, columns);

	std::stringstream ss;
	for(const auto& e: r){
		ss << e << std::endl;
	}
	return ss.str();
}
