# Formatação da etapa 3

Referência: `Apostila de Projeto - Grupo 5.docx`, fornecido em 15/11/2026, SHA-256 `1133155b0edd0e0f0f539687646630f32007e26a20b807d6e54bb563089690d7`.

Os arquivos `ETAPA_3_FORMATADA.docx` e `ETAPA_3_FORMATADA.pdf` contêm a seção 3 inteira: introdução, planejamento original, consolidação 3.1.10 e implementação 3.2 preenchida. A declaração de horas e as demais etapas não fazem parte desses arquivos.

| Elemento | Definição reutilizada do modelo |
|---|---|
| Página | A4, margens de 2,54 cm; cabeçalho/rodapé sem conteúdo adicional. |
| Texto | Times New Roman, 12 pt, justificado, entrelinha de 1,15. Recuos de primeira linha conforme os parágrafos de referência. |
| Título da etapa | Heading 1, 20 pt; numeração no nível 0. |
| Seções 3.1 e 3.2 | Heading 2, 16 pt; numeração no nível 1. |
| Subseções | Heading 3, 14 pt, cor 434343; numeração no nível 2. |
| Subdivisões | Heading 4, 12 pt, cor 666666; numeração no nível 3. |
| Tabelas | Times New Roman 12 pt, texto centralizado, bordas pretas de 1 pt, cabeçalho cinza CCCCCC, largura de 9029 twips e margens internas de 100 twips. |
| Código | Courier New 10,5 pt, alinhado à esquerda, entrelinha simples, em tabela de uma célula com borda preta. |
| Legendas | Times New Roman 12 pt, negrito, centralizadas; sem os prefixos de figura/listagem criados na versão anterior. |
| Imagens | Incorporadas em linha com o texto, preservando proporções e conteúdo das capturas reais. |

As definições de estilos, numeração, propriedades de parágrafos, células e bordas foram extraídas do OOXML do arquivo de referência. Fontes herdadas foram materializadas explicitamente nos novos trechos para reduzir alterações ao importar/copiar. A numeração da lista começa em 3, inclusive quando o DOCX é aberto isoladamente. As linhas das novas tabelas não são divididas entre páginas, e o cabeçalho é repetido nas continuações.

A exportação PDF foi feita a partir do próprio DOCX no LibreOffice, com as fontes Times New Roman e Courier New efetivamente disponíveis e incorporadas ao PDF. O PDF não utiliza o diagramador independente da revisão anterior.

## Reprodução

Dependências do gerador: Python 3, lxml e Pillow. A partir da raiz do repositório:

```bash
python3 Documentos/etapa3/gerar_docx.py --modelo '/caminho/Apostila de Projeto - Grupo 5.docx'
```

O modelo original permanece inalterado. Para o PDF, exportar o DOCX pelo Word, Google Docs ou LibreOffice usando as fontes indicadas. O corpo da seção continua editável, e as tabelas são tabelas nativas do DOCX.

A conferência realizada abrange o OOXML e a abertura/exportação no LibreOffice. Não foi realizada colagem no documento de destino do Google Docs; essa operação pode ajustar paginação e continuidade de listas conforme as configurações do destino. Para conservar os estilos ao copiar, utilizar a colagem com formatação, não a opção de colar apenas texto.
