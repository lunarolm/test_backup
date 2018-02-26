#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

#define true 1
#define false 0
#define typename(x) _Generic((x),long int:"long int", double:"double")

enum{LVAL_NUM, LVAL_ERR};
enum{LERR_DIVIDE_BY_ZERO, LERR_BAD_OPERATOR, LERR_BAD_NUM};

typedef struct{//this is a generic lisp value, which stores any possible value, including err"
	int type;
	long num;
	int err;
}lval;

//builder functions for lvals
lval lval_num(long x){
	lval r;
	r.type = LVAL_NUM;
	r.num = x;
	return r;
}

lval lval_err(int err){
	lval r;
	r.type = LVAL_ERR;
	r.err = err;
	return r;
}

int print_lval(lval val){//simply prints the value of an lval to console. Designed for chaining together.
	switch(val.type){
		case LVAL_NUM: printf("%li", val.num); return 0;

		case LVAL_ERR:
			if(val.err == LERR_DIVIDE_BY_ZERO){
				printf("ERROR: Attempted to divide by zero.\n");
			}
			if(val.err == LERR_BAD_OPERATOR){
				printf("Invalid Operator.\n");
			}
			if(val.err == LERR_BAD_NUM){
				printf("Invalid Number.\n");
			}
			return 0;
		default: return -1;//lval needs a type, so this is an error return
	}
}

int println_lval(lval val){
	int r = print_lval(val);
	printf("\n");
	return r;
}

int count_leaves(mpc_ast_t* inputTree){//abstract syntax tree node counting function, same with the next function
	if(inputTree->children_num == 0){
		return 1;
	}
	else if(inputTree->children_num >=1){
		int tally = 0;
		for(int i = 0; i < inputTree->children_num; i++){
			if(inputTree->children[i]->children_num == 0){
				tally += count_leaves(inputTree->children[i]);
			}
			else{
				count_leaves(inputTree->children[i]);
			}
		}
		return tally;
	}
	return 0;//empty tree or error
}

int count_nodes(mpc_ast_t* inputTree){

	if(inputTree->children_num == 0){
		return 1;
	}
	else if(inputTree->children_num >= 1){
		int tally = 1;
		for(int i = 0; i < inputTree->children_num; i++){
			tally += count_nodes(inputTree->children[i]);
		}
		return tally;
	}
	return 0;//empty tree or error
}

lval eval_op(lval x, char*op, lval y){
	
	if(x.type == LVAL_ERR){return x;}
	if(y.type == LVAL_ERR){return y;}

	if(!strcmp(op, "+") || !strcmp(op, "add")){//if op(erator) == "+"
		return lval_num(x.num + y.num);
	}
	if(!(strcmp(op, "-")) || !strcmp(op, "sub")){
		return lval_num(x.num - y.num);
	}
	if(!strcmp(op, "*") || !strcmp(op, "mul")){
		return lval_num(x.num * y.num);
	}
	if(!strcmp(op, "/") || !strcmp(op, "div")){
		return y.num == 0? lval_err(LERR_DIVIDE_BY_ZERO) : lval_num(x.num / y.num);
	}
	if(!strcmp(op, "%") || !strcmp(op, "mod")){
		return y.num == 0? lval_err(LERR_DIVIDE_BY_ZERO) : lval_num(x.num % y.num);
	}
	if(!strcmp(op, "^") || !strcmp(op, "pow")){
		
		long accumulator = 1;
		for(int i=0; i < y.num; i++){
			accumulator = accumulator * x.num;
		}
		return lval_num(accumulator);
	}
	if(!strcmp(op, "min")){
		if(x.num < y.num){
			return x;
		}
		return y;
	}
	if(!strcmp(op, "max")){
		if(x.num > y.num){
			return x;
		}
		return y;
	}
	return lval_err(LERR_BAD_OPERATOR);//the operator didn't match anything
}

lval eval(mpc_ast_t* inputTree){//evaluate math expressions
	if(strstr(inputTree->tag, "number")){
		errno = 0;
		long x = strtol(inputTree->contents, NULL, 10);
		return errno != ERANGE? lval_num(x) : lval_err(LERR_BAD_NUM);
	}

	//start with the second child, because the first is likely a "regex" or a bracket
	char* op = inputTree->children[1]->contents;

	//next is either an number or an expression. If a number, return it. Else, evaluate again.
	lval x = eval(inputTree->children[2]);

	//if the statement only has two terms of the form - 1 or something, return -1. A special case.
	if(strstr(op, "-") && !strstr(inputTree->children[3]->tag, "expression")){
		x.num = -(x.num);
	}

	int i = 3;
	while(strstr(inputTree->children[i]->tag, "expression")){
		x = eval_op(x,op, eval(inputTree->children[i]));
		i++;
	}

	return x;
	
}

int main(int argc, char** argv){

	//define MPC parsers
	mpc_parser_t *Number 	 = mpc_new("number");
	mpc_parser_t *Operator 	 = mpc_new("operator");
	mpc_parser_t *Expression = mpc_new("expression");
	mpc_parser_t *Lispy 	 = mpc_new("lispy");

	mpca_lang(//language definition
		MPCA_LANG_DEFAULT,
		//grammer definitions for parsers
		"																													\
		number 		: /-?(([0-9]*\\.[0-9]+)|[0-9]+)/;																		\
		operator 	: '+' | '-' | '*' | '/' | '%' | '^' | /add/ | /sub/ | /mul/ | /div/ | /mod/ | /pow/ | /min/ | /max/;	\
		expression 	: <number> | '(' <operator><expression>+ ')';															\
		lispy 		: /^/ <operator><expression>+ /$/;																		\
		",
		Number,
		Operator,
		Expression,
		Lispy);

	printf("~Lispy Alpha~\n");

    while(true){

        char* input = readline(":>>");

        mpc_result_t result;

        if(mpc_parse("<stdin>", input,  Lispy, &result)){//if result is valid
        	mpc_ast_print(result.output);
        	
        	//printf("\nTree Printout\n");
        	mpc_ast_t* parseTree = result.output;
        	
        	/*
        	printf("Tag: %s\n", parseTree->tag);
        	printf("Contents: %s\n", parseTree->contents);
        	printf("Number of Children: %d\n", parseTree->children_num);
        	printf("Number of Nodes: %d\n", count_nodes(parseTree));
        	printf("Number of Leaves: %d\n", count_leaves(parseTree));
        	*/

			printf("\nStatement Returned: "); println_lval(eval(parseTree));
			mpc_ast_delete(result.output);
        }
        else{
        	mpc_err_print(result.error);
        	mpc_err_delete(result.error);
        }

        free(input);

    }

    mpc_cleanup(4, Number, Operator, Expression, Lispy);
    return 0;
}
