int soma(int valores[], int tamanho) {
    int i;
    int total = 0;
    for (i = 0; i < tamanho; i = i + 1) {
        total = total + valores[i];
    }
    return total;
}

int main() {
    int valores[5];
    int i;
    for (i = 0; i < 5; i = i + 1) read(valores[i]);
    print(soma(valores, 5));
    return 0;
}
