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
| `luz <dado> on=<png> off=<png> [x= y=] [inverte=sim]` | Mostra o PNG aceso quando o dado está ligado |
| `texto <dado> x= y= altura= cor=RRGGBB alinha= fonte= [casas=] [sufixo=]` | Número ou texto; `y` é a linha de base |
| `barra <dado> img=<png cheio> de= ate= sentido=direita/esquerda/cima/baixo` | Revela a imagem conforme o valor |
| `arco <dado> img=<png cheio> cx= cy= de= ang_de= ate= ang_ate=` | Revela um setor circular |
| `ponteiro <dado> img=<png> cx= cy= de= ang_de= ate= ang_ate= [pivo_x= pivo_y=]` | Gira a imagem (apontando para cima) em volta do pivô |

Ângulos em graus: 0 aponta para cima e cresce no sentido horário.

## Dados
**Luzes:** `seta_esq seta_dir alerta lanterna farol_baixo farol_alto farol_aux farol_teto giroflex freio_mao freio_motor retarder cruzeiro diferencial eixo eixo_carreta luz_freio re limpador motor limite carga aviso_combustivel aviso_arla aviso_ar emergencia_ar aviso_oleo aviso_agua aviso_bateria sempre`

**Valores:** `velocidade rpm combustivel arla ar agua oleo pressao_oleo bateria cruzeiro limite acelerador freio embreagem autonomia hodometro consumo retarder litros nav_km carga`

**Só texto:** `marcha hora nav_tempo caminhao destino`

Com a ignição desligada todas as luzes apagam e os textos somem. Erros no arquivo aparecem na tela do app com o número da linha.
