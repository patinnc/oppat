// from http://www.firmcodes.com/write-a-program-to-check-for-balanced-parentheses-in-an-expression-in-c/
// don't know the license
#include <stdio.h>
#include<iostream>
#include<stdlib.h>
#include<vector>
#include<stack>			//Use Standard template library to create Stack data structure 
using namespace std;
#include "ck_nesting.h"

/* Driver functions */
bool CheckForBalancedParenthesis(const char s[], int &last, std::vector <lst_fld_beg_end_str> &fld_beg_end, int verbose);
bool Match(char char_1, char char_2);

/* Main Method */
int ck_parens(const char str[], int &last, std::vector <lst_fld_beg_end_str> &fld_beg_end, int verbose)
{
	if(CheckForBalancedParenthesis(str, last, fld_beg_end, verbose)) {
		if (verbose > 0)
			printf("Parenthesis are balanced \n");
		return 1;
	} else {
		printf("Parenthesis are NOT balanced \n");
		return 0;
	}
}

/* Return 1 Parenthesis has balanced  */
bool CheckForBalancedParenthesis(const char s[], int &last, std::vector <lst_fld_beg_end_str> &fld_beg_end, int verbose)
{
	/* Declare an character Stack using STL */
	stack<int> pos;
	stack<char> Stack;
	int i=0;
	int prv_fld_beg = -1;
	
	/* Traverse the given string or expresstion to check matching parenthesis */
	while(s[i])
	{
		
		/*If the exp[i] is a starting parenthesis then push it to Stack*/
		if( s[i]=='(' || s[i]=='{' || s[i]=='[' )
		{
			if (pos.size() == 0) {
				prv_fld_beg = i;
			}
			pos.push(i);
			Stack.push(s[i]);
		}
		
		/* If exp[i] is a ending parenthesis then check for empty stack or 
		paranthesis matching then pop it from Stack*/
		if( s[i]==')' || s[i]=='}' || s[i]==']' )
		{
			if( Stack.empty() || !Match(Stack.top(),s[i]) )
			{
				return false;
			}
			else
			{
				int j = pos.top();
				//prv_fld_beg = j;
				pos.pop();
				if (verbose > 0)
					printf("ck char= %c, beg= %d, end= %d, pos.size()= %d\n", Stack.top(), j, i, (int)pos.size());
				Stack.pop();
				if( Stack.empty()) {
					last = i;
					break;
				}
			}
		}
		if (s[i] == ',' && pos.size() == 1) {
			lst_fld_beg_end_str fbes;
			fbes.beg = prv_fld_beg;
			fbes.end = i;
			fld_beg_end.push_back(fbes);
			if (verbose > 0)
				printf("fld dlm , at %d-%d\n", prv_fld_beg, i);
			prv_fld_beg = i;
		}
		i++;
	}
	if (prv_fld_beg < i-1) {
		if (verbose > 0)
			printf("fld dlm at %d-%d\n", prv_fld_beg, i-1);
		lst_fld_beg_end_str fbes;
		fbes.beg = prv_fld_beg;
		fbes.end = i;
		fld_beg_end.push_back(fbes);
	}
	last = i;
	
	/*If Stack is empty then paranthesis are balanced otherwise NOT */
	return Stack.empty();
}

/* Match for relevent paranthesis */
bool Match(char char_1, char char_2)
{
	if( char_1=='(' && char_2==')' )
		return true;
	else if(char_1=='{' && char_2=='}')
		return true;
	else if(char_1=='[' && char_2==']')
		return true;
	else
		return false;
}
