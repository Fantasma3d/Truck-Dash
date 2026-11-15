# Diário de aprendizado — TruckDash

## 14/11/2026 — do zero ao painel funcionando

### 1. Engenharia reversa de um app pronto
- Analisei o APK do **SIM Dashboard** para entender como esse tipo de app funciona:
  - **No PC:** um servidor .NET instala um plugin próprio dentro do ETS2 e se anuncia na rede por **mDNS** (`_simdb._udp`).
  - **No celular:** o app exige **Android 5+** (`minSdk 21`) e licença, por isso não rodaria no meu tablet com Android 4.
- **Lição:** antes de copiar uma ideia, entender a arquitetura dela, ou seja, quem fala com quem e por qual caminho.

### 2. Telemetria do ETS2 (SCS SDK)
- O jogo carrega DLLs da pasta `bin\win_x64\plugins` e chama `scs_telemetry_init`.
- O plugin se registra em **eventos**: início e fim de quadro, pausa e configuração.
  - Os **canais** (como `truck.speed` e `truck.engine.rpm`) entregam os valores numéricos.
  - O evento de **configuração** traz os dados fixos: marca, modelo, capacidade do tanque, rpm máximo.
- **Lição:** ler a especificação oficial com cuidado. O tamanho e a ordem dos campos das estruturas precisam bater exatamente.

### 3. Redes: UDP, broadcast, unicast e TCP
- **Broadcast** (`x.x.x.255`) só alcança a mesma sub-rede. Meu PC (`192.168.213.x`) e o tablet (`192.168.18.x`) estavam em redes diferentes, por isso os dados não chegavam.
- **Descoberta automática:** o tablet manda um "oi" e o PC passa a mandar direto para ele.
- **USB via ADB:** `adb forward tcp:45456 tcp:45456` cria um túnel TCP pelo cabo.
- **TCP é um fluxo de bytes, não de mensagens:** cada pacote precisa de tamanho conhecido para ser remontado do outro lado.

### 4. Android sem Android Studio
- Uma `NativeActivity` permite fazer um app inteiro em C, sem Java.
- Montei o APK "na mão":
  - o manifesto em XML **binário**;
  - a tabela de recursos (`resources.arsc`);
  - as bibliotecas `.so` para ARM e x86;
  - a assinatura **SHA1**, porque o Android anterior ao 4.3 não aceita SHA‑256.
- **Lição:** um APK é só um ZIP com regras. Conhecer o formato permite compilar sem as ferramentas oficiais.

### 5. Gráficos por software
- Desenhar círculos, arcos, polígonos e texto com **antialiasing** pixel a pixel.
- **Otimização:** o fundo (mostradores e números) é desenhado uma vez só. A cada quadro só os ponteiros e os valores são redesenhados.

### 6. Depuração de verdade
- O jogo "não abria": pelo `game.log.txt` e pelo `game.crash.txt` descobri que o culpado era um **mod** ainda sendo baixado, e não o plugin.
- **Lição:** ler os logs antes de culpar o código novo.

## 14/11/2026 (noite) — painéis de verdade

### 7. Respeitar a proteção dos outros
- O SIM Dashboard baixa os painéis de um servidor e os criptografa (`libsimdb.so`). Os mods da Scania e do FH6 também eram protegidos.
- **Decisão:** não quebrar a proteção de ninguém. Usar a fonte original: os arquivos do próprio jogo, abertos com o extrator.

### 8. Engenharia reversa de um formato de UI
- `ui/dashboard/*.sii`: uma árvore de janelas, grupos, textos, barras e ponteiros, com coordenadas, camadas e IDs.
- **Mini-linguagem de marcação:** `<img>`, `<font>`, `<color>`, `<offset>`, `<align>`, `<ret>`, `<margin>`.
  - `<ret>` volta ao canto do elemento; as posições vêm todas dos `<offset>`.
  - Coordenadas podem ser frações (`0.5`) ou pixels (`p256`).
- **Fontes SDF/MSDF:** a distância até a borda é a mediana dos canais R, G e B, o que mantém o texto nítido em qualquer tamanho.
- **Ponteiros:** a textura inteira gira em volta de um pivô, a 1 unidade do painel por pixel.
- **Barras:** o retângulo cresce conforme o valor e corta o conteúdo.
- **Lição:** escrever primeiro uma prévia em Python, comparar com o jogo e só depois portar para C.

### 9. Bugs que ensinaram algo
- A ordem de desenho importa: uma "máscara" desenhada por cima de uma barra cria os segmentos. Se a máscara vai para o fundo em cache, a barra vira um bloco sólido.
- Um fundo escuro semitransparente não é um fundo opaco: precisa considerar o alfa da cor.
- O script de build montou o APK com a biblioteca antiga depois de uma falha de compilação. Agora ele apaga a biblioteca antes de compilar.

## Próximos passos
- [ ] Testar no tablet real e medir o FPS
- [ ] Integrar com o **button box** (Arduino Nano)
- [x] Painéis originais do jogo
- [ ] Ajustar os painéis antigos que ainda têm sobreposições (MAN eTGX, Actros 2014)
- [ ] Modo noturno
