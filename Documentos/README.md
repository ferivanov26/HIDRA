# Documentação do projeto

[Apostila de Projeto - Grupo 5.pdf](Apostila%20de%20Projeto%20-%20Grupo%205.pdf) é o relatório completo. A revisão de 15/11/2026 incorpora a implementação e a validação da etapa 3, a partir do relatório da main `b5fd031` e do software `6573a7d`.

Os arquivos editáveis e as evidências utilizadas estão em `etapa3/`:

- [ETAPA_3.md](etapa3/ETAPA_3.md): consolidação do planejamento (3.1.10), implementação (3.2.1–3.2.8) e declaração de horas preservada.
- [ETAPA_3_IMPLEMENTACAO.pdf](etapa3/ETAPA_3_IMPLEMENTACAO.pdf): leitura isolada da atualização, sem a declaração de horas.
- `figuras/`: dez capturas reais do aplicativo, dois diagramas e a identificação/hash dos arquivos de origem. As capturas não foram retocadas.
- [gerar_relatorio.py](etapa3/gerar_relatorio.py): composição dos PDFs a partir do Markdown, imagens e base preservada no histórico Git.

A composição mantém integralmente as páginas 1–64 da base e substitui a página 65, que continha apenas os títulos vazios da seção 3.2 e a declaração de horas. Os valores declarados pelos integrantes permanecem os mesmos; não foram estimadas novas horas. A consolidação 3.1.10 explicita as extensões do contrato sem reescrever o planejamento original ou as contribuições das etapas 1 e 2.

## Reprodução dos PDFs

Dependências: Python 3, ReportLab, fontes Liberation e Poppler (`pdfinfo`, `pdftotext`, `pdfseparate` e `pdfunite`). Na raiz do repositório:

```bash
python3 Documentos/etapa3/gerar_relatorio.py
```

O script recupera do Git o PDF original da main `b5fd031`, verifica seu SHA-256 e compara o texto das páginas preservadas antes de substituir o relatório completo. Em um checkout sem esse histórico, `--base /caminho/relatorio-original.pdf` aceita o mesmo arquivo de referência; o PDF já atualizado não pode ser utilizado como base.

As capturas são do aplicativo 0.2.0 em um aparelho real. Os gráficos representam a implementação, e as listagens/logs se referem ao código e aos ensaios registrados em [VALIDACAO.md](../HIDRA_Etapa3/docs/VALIDACAO.md). A seção de resultados distingue testes automatizados, ensaios reais de comunicação, consulta no telefone e validações físicas ainda pendentes.
