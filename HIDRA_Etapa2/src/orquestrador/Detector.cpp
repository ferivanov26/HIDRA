#include "Detector.h"
#include <math.h>

namespace {
inline uint32_t desde(uint32_t agora, uint32_t t) { return agora - t; } // seguro no overflow de millis()
}

void Detector::iniciar(bool chave_acionada, bool botao_pressionado, uint32_t agora) {
  fis_ = {};
  fis_.copo_na_base = copoNaBaseDe(chave_acionada);
  chave_bruta_copo_na_base_ = fis_.copo_na_base;
  t_chave_bruta_ = agora;
  fis_.botao_pressionado = botao_pressionado;
  longo_emitido_ = botao_pressionado; // botao preso no boot nao vira comando
  t_botao_ = agora;
  n_ = 0; ini_ = 0;
  tem_anterior_ = false;
  abaixo_vazia_ = false;
  indisponibilidade_reportada_ = false;
}

void Detector::entrada(const LeituraFisica& e, FilaOcorrencias& saida) {
  switch (e.tipo) {
    case EntradaFisica::BotaoPressionado:
      fis_.botao_pressionado = true;
      t_botao_ = e.t_ms;
      longo_emitido_ = false;
      break;
    case EntradaFisica::BotaoLiberado:
      if (fis_.botao_pressionado && !longo_emitido_) {
        saida.push({Observacao::BotaoCurto, e.t_ms, 0.0f});
      }
      fis_.botao_pressionado = false;
      break;
    case EntradaFisica::ChaveAcionada:
    case EntradaFisica::ChaveLiberada:
      chave_bruta_copo_na_base_ = copoNaBaseDe(e.tipo == EntradaFisica::ChaveAcionada);
      t_chave_bruta_ = e.t_ms;
      break;
    case EntradaFisica::MassaAtualizada:
      novaMassa(e.massa_g, e.t_ms, saida);
      break;
    case EntradaFisica::MassaIndisponivel:
      fis_.massa_disponivel = false;
      fis_.massa_estavel = false;
      n_ = 0; ini_ = 0; tem_anterior_ = false; abaixo_vazia_ = false;
      if (!indisponibilidade_reportada_) {
        indisponibilidade_reportada_ = true;
        saida.push({Observacao::SensorMassaIndisponivel, e.t_ms, 0.0f});
      }
      break;
  }
}

void Detector::tick(uint32_t agora, FilaOcorrencias& saida) {
  // Microchave: so confirma apos confirmacao_chave_ms sem nova mudanca.
  if (chave_bruta_copo_na_base_ != fis_.copo_na_base &&
      desde(agora, t_chave_bruta_) >= p_.confirmacao_chave_ms) {
    fis_.copo_na_base = chave_bruta_copo_na_base_;
    saida.push({fis_.copo_na_base ? Observacao::CopoVoltouABase : Observacao::CopoSaiuDaBase,
                agora, 0.0f});
  }
  // Botao longo: emitido enquanto ainda pressionado (feedback imediato).
  if (fis_.botao_pressionado && !longo_emitido_ &&
      desde(agora, t_botao_) >= p_.botao_longo_ms) {
    longo_emitido_ = true;
    saida.push({Observacao::BotaoLongo, agora, 0.0f});
  }
}

void Detector::novaMassa(float m, uint32_t t, FilaOcorrencias& saida) {
  if (!isfinite(m)) return;
  if (!fis_.massa_disponivel) {
    fis_.massa_disponivel = true;
    if (indisponibilidade_reportada_) {
      indisponibilidade_reportada_ = false;
      saida.push({Observacao::SensorMassaRecuperado, t, m});
    }
  }
  fis_.massa_atual_g = m;

  // Movimento: variacao ACUMULADA desde a ultima referencia (pega tambem um
  // despejo lento, de poucos gramas por amostra). A referencia e realinhada
  // quando a massa fica estavel, para que deriva lenta nao acumule.
  if (!tem_anterior_) {
    anterior_g_ = m;
    tem_anterior_ = true;
  } else if (fabsf(m - anterior_g_) >= p_.movimento_g) {
    anterior_g_ = m;
    if (desde(t, t_ultimo_movimento_) >= 500) {
      t_ultimo_movimento_ = t;
      saida.push({Observacao::MassaEmMovimento, t, m});
    }
  }

  // Janela de estabilidade
  janela_[(ini_ + n_) % JANELA] = {t, m};
  if (n_ < JANELA) ++n_; else ini_ = (uint8_t)((ini_ + 1) % JANELA);
  while (n_ > 0 && desde(t, janela_[ini_].t) > p_.janela_estavel_ms) {
    ini_ = (uint8_t)((ini_ + 1) % JANELA); --n_;
  }
  float mn = m, mx = m, soma = 0.0f;
  for (uint8_t i = 0; i < n_; ++i) {
    const float v = janela_[(ini_ + i) % JANELA].m;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    soma += v;
  }
  // >= 3 amostras cobrindo a janela inteira (200 ms entre amostras)
  const bool cobre = n_ >= 3 && desde(t, janela_[ini_].t) + 250 >= p_.janela_estavel_ms;
  fis_.massa_estavel = cobre && (mx - mn) <= p_.tolerancia_estavel_g;
  if (fis_.massa_estavel) fis_.massa_estavel_g = soma / n_;

  if (fis_.massa_estavel) {
    fis_.tara_suspeita = fis_.massa_estavel_g <= -p_.limiar_ocupada_g;
    anterior_g_ = fis_.massa_estavel_g; // referencia do detector de movimento
  }

  // Ocupacao com histerese
  if (!fis_.plataforma_ocupada) {
    if (fis_.massa_estavel && fis_.massa_estavel_g >= p_.limiar_ocupada_g) {
      fis_.plataforma_ocupada = true;
      abaixo_vazia_ = false;
      ultimo_estavel_reportado_g_ = fis_.massa_estavel_g;
      saida.push({Observacao::PlataformaOcupada, t, fis_.massa_estavel_g});
    }
    return;
  }
  if (m <= p_.limiar_vazia_g) {
    if (!abaixo_vazia_) { abaixo_vazia_ = true; t_abaixo_vazia_ = t; }
    if (desde(t, t_abaixo_vazia_) >= p_.confirmacao_vazia_ms) {
      fis_.plataforma_ocupada = false;
      abaixo_vazia_ = false;
      saida.push({Observacao::PlataformaVazia, t, m});
    }
    return;
  }
  abaixo_vazia_ = false;
  if (fis_.massa_estavel &&
      fabsf(fis_.massa_estavel_g - ultimo_estavel_reportado_g_) >= p_.passo_estavel_g) {
    ultimo_estavel_reportado_g_ = fis_.massa_estavel_g;
    saida.push({Observacao::MassaEstavel, t, fis_.massa_estavel_g});
  }
}
