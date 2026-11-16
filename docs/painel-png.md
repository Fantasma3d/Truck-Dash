# Formato do painel por camadas PNG

Pasta no tablet: `/sdcard/TruckDash/scania-smart-dash/`, com o `painel.txt` e os PNGs.

## Imagens
- PNG de 8 bits (16 bits também é aceito), com transparência, **sem entrelaçamento**.
- As posições são em pixels do `fundo.png`. O painel é ajustado à tela mantendo a proporção.
- Uma camada exportada no tamanho do fundo não precisa de `x=`/`y=`. Um recorte pequeno precisa.

## Linhas do `painel.txt`
Linhas que começam com `#` são comentários. Os itens são desenhados na ordem do arquivo.

| Linha | O que faz |
|---|---|
| `fundo <png>` | Imagem de fundo; define o tamanho do painel |
| `tela todas` ou `tela 1 3` | As linhas seguintes valem só para essas telas do carrossel |
| `imagem <png> [x= y=]` | Imagem fixa (rótulos, ícones, indicador da tela) |
| `luz <dado> on=<png> off=<png> [x= y=] [inverte=sim]` | Mostra o PNG aceso quando o dado está ligado |
| `texto <dado> x= y= altura= cor=RRGGBB alinha= fonte= [casas=] [sufixo=] [vazio=]` | Número ou texto; `y` é a linha de base; `vazio` aparece quando não há valor (ex.: sem limite) |
| `barra <dado> img=<png cheio> de= ate= sentido=direita/esquerda/cima/baixo` | Revela a imagem conforme o valor |
| `arco <dado> img=<png cheio> cx= cy= de= ang_de= ate= ang_ate=` | Revela um setor circular |
| `ponteiro <dado> img=<png> cx= cy= de= ang_de= ate= ang_ate= [pivo_x= pivo_y=]` | Gira a imagem (apontando para cima) em volta do pivô |

Ângulos em graus: 0 aponta para cima e cresce no sentido horário.

## Dados
**Luzes:** `seta_esq seta_dir alerta lanterna farol_baixo farol_alto farol_aux farol_teto giroflex freio_mao freio_motor retarder cruzeiro diferencial eixo eixo_carreta luz_freio re limpador motor limite carga carreta pausado ponto_cego_esq ponto_cego_dir faixa_esq faixa_dir aviso_combustivel aviso_arla aviso_ar emergencia_ar aviso_oleo aviso_agua aviso_bateria sempre`

**Valores:** `velocidade rpm combustivel arla ar agua oleo pressao_oleo bateria cruzeiro limite acelerador freio embreagem autonomia hodometro consumo retarder litros nav_km carga viagem_km viagem_vel consumo_hora consumo_inst arla_litros desgaste`

**Só texto:** `marcha hora nav_tempo viagem_tempo caminhao destino`

`viagem_*` contam desde que o app abriu; `consumo_hora` (l/h) é calculado no tablet pela queda do tanque.

**Desempenho:** cada tela tem o próprio cache (fundo + imagens fixas + luzes apagadas), montado só quando a tela muda.

Com a ignição desligada todas as luzes apagam e os textos somem. Erros no arquivo aparecem na tela do app com o número da linha.
