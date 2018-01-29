#include <stdio.h>


static const int f = 1 << 14;


//x, y are fixed point numbers
//p.q format where p+q = 31, f is 1<<q, (p ==17, q==14)
//n is an integer



int cvt_i2f(int);
int cvt_f2i(int);
int cvt_f2i_floor(int x);

int add_f_i(int, int);
int sub_f_i(int, int);
int sub_i_f(int, int);

int mul_f_f(int, int);
int mul_f_i(int, int);
int mul_i_i(int, int);

int div_f_f(int, int);
int div_f_i(int, int);
int div_i_i(int, int);

int cvt_i2f(int n){
    return n*f;
}

//convert fixed point num to integer
// round result to nearest
int cvt_f2i(int x){
    
    if(x >=0)
        return (x+f/2) / f;
    else
        return (x-f/2) / f;
}

// round down
int cvt_f2i_floor(int x){
    return x/f;
}


// all below functions' return type is fixed point!!

int add_f_i(int x, int n){
    return x+n*f;
}

int sub_f_i(int x, int n){
    return x-n*f;
}
int sub_i_f(int n, int x){
    return n*f-x;
}

int mul_f_f(int x, int y){
    return ((int64_t)x) * y / f;
}

int mul_f_i(int x, int n){
    return x*n;
}

int mul_i_i(int a, int b){
    return cvt_i2f(a) * b; 
}

int div_f_f(int x, int y){
    return ((int64_t)x)*f/y;
}

int div_f_i(int x, int n){
    return x/n;
}

int div_i_i(int a, int b){
    return cvt_i2f(a)/b;
}

