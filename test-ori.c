int foo(int var_a){
    return var_a;
}
int indirect_version(int (*fn)(int)){
    int var_i, var_b = 0;
    for(var_i = 0; var_i < 3; ++var_i){
        var_b = fn(var_b);
    }
    return (var_b + var_i + 7);
}
char* swap(char **p, char **q){
  char* t = *p;
  *p = *q;
  *q = t;
  return t;
}
char* swap1(char **p, char **q){
  return *p;
}
char ppp;
int main(){
  char a1, b1;
  char *a = &a1;
  char *b = &b1;
  char *c = swap(&a,&b);
  char **x = &a;
  char* (*func)(char **p, char **q);
  func = swap;
  char *d = (*func)(&a, &b);
  func = swap1;
  **x = b1;
  return indirect_version(&foo);
}
