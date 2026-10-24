int main() {
    int n;
    read(n);
    int i = 0;
    int x = 6;
    int y = 7;
    int soma = 0;
    while (i < n) {
        soma = soma + x * y;
        i = i + 1;
    }
    print(soma);
    return 0;
}
