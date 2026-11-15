# Configuração do HIDRA no Firebase Spark

Este projeto usa somente Authentication por e-mail/senha e uma instância padrão do Realtime Database. O aplicativo consulta dados; o firmware publica por REST. Não é necessário contratar serviços adicionais.

## Plano gratuito

Crie o projeto no [Console Firebase](https://console.firebase.google.com/) e mantenha o plano **Spark**. Não vincule conta de faturamento, cartão, crédito promocional do Google Cloud ou plano Blaze. Vincular Cloud Billing muda o projeto para Blaze. Google Analytics é opcional e pode permanecer desativado.

Conforme a documentação consultada em 14/11/2026, o Spark não cobra uso do RTDB e oferece 1 GB armazenado, 10 GB/mês de downloads e até 100 conexões simultâneas. Esses limites são suficientes para ensaios pequenos, mas não representam capacidade ilimitada; ao atingi-los, há restrição de serviço. Consulte **Uso** no Console. Este projeto não requer Cloud Functions, Cloud Storage, SMS, Extensions ou App Hosting.

Fontes: [planos Firebase](https://firebase.google.com/docs/projects/billing/firebase-pricing-plans), [uso e cobrança do RTDB](https://firebase.google.com/docs/database/usage/billing), [limites de conexões](https://firebase.google.com/docs/database/usage/limits).

## 1. Criar o projeto e o banco

1. Crie um projeto com nome HIDRA e um ID disponível, por exemplo `hidra-grupo5-<sufixo>`. Anote o **ID do projeto**.
2. Confirme que a tela de plano mostra **Spark**.
3. Em **Build > Realtime Database**, crie o banco padrão. Escolha uma região disponível no Spark e anote a **URL exata** exibida. A URL varia com a região; não a deduza pelo nome do projeto.
4. Escolha inicialmente o modo bloqueado. Não use regras públicas de teste.
5. Na aba **Regras**, substitua o conteúdo pelo arquivo inteiro `firebase/database.rules.json` e publique. Este é o arquivo de **Realtime Database**, não de Firestore.

As regras mantêm a raiz bloqueada. Sem as contas e os vínculos abaixo, o acesso do aplicativo e do dispositivo será recusado, como esperado.

## 2. Criar as identidades

Em **Authentication > Sign-in method**, habilite somente **E-mail/senha**. Não é necessário habilitar login por link nem telefone.

Em **Authentication > Users**, cadastre:

| Conta | Uso | Credencial no projeto |
|---|---|---|
| Dispositivo físico | ESP32 `hidra_01` | Somente `firmware/iot/include/secrets.h` |
| Simulador | Publicador `sim_hidra_01` | Variáveis locais do terminal |
| Cuidador | Aplicativo Android | Digitada na tela de acesso |

Use e-mails distintos sob seu controle e senhas próprias. Não reutilize as senhas fictícias dos emuladores. Copie o UID de cada usuário. Nenhuma conta de serviço ou chave privada será colocada na placa ou no aplicativo.

## 3. Vincular contas e dispositivos

Na aba **Dados** do Realtime Database, cadastre os nós abaixo com os UIDs reais. Em um banco novo e vazio, é possível importar este JSON após substituir os marcadores. **Não importe sobre um banco com histórico: a importação na raiz substitui dados.** Nesse caso, adicione apenas os nós necessários pelo Console.

```json
{
  "deviceOwners": {
    "hidra_01": "UID_CONTA_ESP32",
    "sim_hidra_01": "UID_CONTA_SIMULADOR"
  },
  "caregiverDevices": {
    "UID_CONTA_CUIDADOR": {
      "hidra_01": true,
      "sim_hidra_01": true
    }
  },
  "devices": {
    "hidra_01": {
      "metadata": {"label": "HIDRA - bancada", "schema_version": 1, "source": "hardware"}
    },
    "sim_hidra_01": {
      "metadata": {"label": "HIDRA - simulador", "schema_version": 1, "source": "simulator"}
    }
  }
}
```

Se estiver enviando eventos fictícios pela ESP32, use uma identidade de ensaio separada, com `metadata/source` igual a `simulator`; não misture essa sessão com os registros dos sensores. Os vínculos são configurados pelo Console, não pelo APK.

## 4. Configurar o aplicativo Android

1. Em **Configurações do projeto > Seus aplicativos**, registre um aplicativo Android com pacote **`br.usp.hidra`** (variante `cloud`).
2. Baixe o `google-services.json` apenas para consultar os valores. O projeto inicializa o SDK por `FirebaseOptions` e não exige adicionar o plugin Google Services.
3. Copie `android/firebase.example.properties` para `android/firebase.local.properties`, ignorado pelo Git, e preencha:

```properties
projectId=ID_REAL_DO_PROJETO
apiKey=VALOR_DE_current_key
applicationId=VALOR_DE_mobilesdk_app_id
databaseUrl=URL_EXATA_DO_REALTIME_DATABASE
```

`applicationId` nesse arquivo é o **ID de aplicativo Firebase**, normalmente começando com `1:`; não é o pacote `br.usp.hidra`. Os campos `current_key` e `mobilesdk_app_id` estão no JSON do aplicativo. Não coloque e-mail nem senha nesse arquivo.

```bash
bash scripts/build_android.sh
adb install -r android/app/build/outputs/apk/cloud/debug/app-cloud-debug.apk
adb shell am start -n br.usp.hidra/br.usp.hidra.MainActivity
```

Entre com a conta do cuidador. Antes da primeira publicação, o banco pode mostrar dispositivos sem estado ou histórico. A variante `cloud` sem configuração apresenta uma mensagem de configuração pendente.

## 5. Configurar e preparar a ESP32

Copie `firmware/iot/include/secrets.example.h` para `secrets.h`, se ainda não existir, e preencha URL, chave de API, e-mail e senha da **conta do dispositivo correspondente ao device_id**. A rede já pode estar configurada localmente. Não publique esse arquivo. O certificado público permanece em `firebase_ca.h`.

Verifique a porta com `pio device list`. A placa usada nesta revisão foi localizada em `/dev/ttyACM0`; a porta pode mudar.

```bash
pio run -d firmware -e esp32-network-probe -t upload --upload-port /dev/ttyACM0
pio device monitor -p /dev/ttyACM0 -b 115200
```

O diagnóstico verifica Wi-Fi, UTC e TLS, sem publicar no banco nem acionar sensores. Um HTTP 404 na raiz de `identitytoolkit.googleapis.com` é uma resposta HTTP válida após o handshake TLS; não comprova autenticação.

**Somente na primeira preparação do LittleFS**, depois de preservar dados existentes, execute:

```bash
bash scripts/prepare_littlefs.sh --initialize-empty /dev/ttyACM0
```

Esse comando substitui o conteúdo da partição LittleFS. Não o use para atualizar um firmware com pendências: o diário seria apagado. O firmware normal não formata automaticamente uma partição que falhou ao montar.

Depois, para o ensaio com eventos fictícios, selecione uma conta/dispositivo de ensaio provisionada como `simulator`:

```bash
pio run -d firmware -e esp32-synthetic -t upload --upload-port /dev/ttyACM0
pio device monitor -p /dev/ttyACM0 -b 115200
```

Envie `s` para diagnóstico e `n` para a sessão fictícia de sete eventos. `auth=1`, `pending=0` e a existência de eventos/resumo no banco são verificações complementares. A aceitação na fila, sozinha, não comprova entrega.

## 6. Validar a nuvem sem sensores

O simulador usa a conta própria e o dispositivo `sim_hidra_01`. Não deixe `FIREBASE_AUTH_EMULATOR_HOST` definido ao testar a nuvem. Configure `HIDRA_DATABASE_URL`, `HIDRA_API_KEY`, `HIDRA_DEVICE_EMAIL` e `HIDRA_DEVICE_PASSWORD` somente no ambiente local. Use um diário novo por projeto/identidade:

```bash
python3 -m simulator --scenario alert --publish --journal .runtime/cloud-simulator.db
```

Confira dez eventos, resumo `completed`, uma instrução, um alerta e ausência de alertas ativos após a resolução. O cuidador deve ler esse dispositivo e uma conta sem vínculo deve receber recusa. Consulte o README para os ensaios equivalentes nos emuladores.

## 7. Manter o uso dentro do Spark

Mantenha apenas os listeners das telas abertas, use a paginação de 20 sessões e não transmita continuamente as amostras do HX711. No Console, confira armazenamento, downloads e conexões. Ao terminar os ensaios, remova administrativamente os dados fictícios e as contas que não serão mais usadas. Não habilite faturamento para contornar um limite: reduza ou suspenda os testes e aguarde a renovação da cota aplicável.

A criação do projeto, o cadastro das contas e a configuração local ainda precisam ser feitos pelo responsável. Nada foi provisionado em nuvem nesta revisão.
