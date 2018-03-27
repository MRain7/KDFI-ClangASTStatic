#include <stdlib.h>
#include <stdio.h>
int j;
int func1(int a, int *b, int *c) {
        int i=0;
        j=a++;
        int *d = b;
        b = c;
        c = d;
        while(i!=j){
                *(b+i) = *(c+i) + i; //unsafe op
                i++;
        }
        return i;
}

int func2(int a, int *b, int **c) {
        if(a==0)
                a += 1;
        else
                *c = b;
        return a;
}

int main() {
        int a = 1, b, c;
        int s[10] = {1};
        int *p = malloc(20*sizeof(10));
        int **q = &p+1;
        b = a++;
        c = s[0] + a + (++b);
        **q = s[0];
        s[1] = *(*q+c);

        j = func1(c, s, p); //s, p will not be modified by func1

        for(int i = 0; i<c; i++)
                s[i] = *(p+i); //unsafe op

        *(*q+a) = func2(b, s, q); //q will not change, but *q will change

        return b;
}





//b = a++;
        //c = s[0] + a + (++b);
        //**q = s[0];
        //s[1] =*(*q+a);
        //s[1] =*(*(q+b+*x)+a);
        //s[1] = **q + *x + a;
        //c = s[0] + a + (++b);
        //a = s[1];
        //j = func1(c, s, p) + a; //s, p will not be modified by func1
        //a = b+c;
        //for(int i = 0; i<c; i++)
        //        s[i] = *(p+i); //unsafe op
        //*(p+=2) = 2;*(*q+a)
        //*(*(b+q)+*p+a+c)
        //*p = a;
        //*(*q+a) = func2(b, s, q); //q will not change, but *q will change
        //*(a+*(q+b))