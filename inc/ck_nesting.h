// from http://www.firmcodes.com/write-a-program-to-check-for-balanced-parentheses-in-an-expression-in-c/
// don't know the license
#pragma once 
struct lst_fld_beg_end_str {
	int beg, end;
};
int ck_parens(const char str[], int &last, std::vector <lst_fld_beg_end_str> &fld_beg_end, int verbose);
