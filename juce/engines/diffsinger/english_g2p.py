# Adapted from g2p-en 2.1.0, Kyubyong Park and Jongseok Kim.
# Apache-2.0; see pronunciation-data/LICENSE-g2p-en.txt.
# Changes: offline predictor only; no NLTK imports/downloads.
import os
import numpy as np
dirname = os.path.join(os.path.dirname(__file__), "pronunciation-data")

class EnglishPredictor(object):

    def __init__(self):
        super().__init__()
        self.graphemes = ['<pad>', '<unk>', '</s>'] + list('abcdefghijklmnopqrstuvwxyz')
        self.phonemes = ['<pad>', '<unk>', '<s>', '</s>'] + ['AA0', 'AA1', 'AA2', 'AE0', 'AE1', 'AE2', 'AH0', 'AH1', 'AH2', 'AO0', 'AO1', 'AO2', 'AW0', 'AW1', 'AW2', 'AY0', 'AY1', 'AY2', 'B', 'CH', 'D', 'DH', 'EH0', 'EH1', 'EH2', 'ER0', 'ER1', 'ER2', 'EY0', 'EY1', 'EY2', 'F', 'G', 'HH', 'IH0', 'IH1', 'IH2', 'IY0', 'IY1', 'IY2', 'JH', 'K', 'L', 'M', 'N', 'NG', 'OW0', 'OW1', 'OW2', 'OY0', 'OY1', 'OY2', 'P', 'R', 'S', 'SH', 'T', 'TH', 'UH0', 'UH1', 'UH2', 'UW', 'UW0', 'UW1', 'UW2', 'V', 'W', 'Y', 'Z', 'ZH']
        self.g2idx = {g: idx for idx, g in enumerate(self.graphemes)}
        self.idx2g = {idx: g for idx, g in enumerate(self.graphemes)}
        self.p2idx = {p: idx for idx, p in enumerate(self.phonemes)}
        self.idx2p = {idx: p for idx, p in enumerate(self.phonemes)}
        self.load_variables()

    def load_variables(self):
        self.variables = np.load(os.path.join(dirname, 'checkpoint20.npz'))
        self.enc_emb = self.variables['enc_emb']
        self.enc_w_ih = self.variables['enc_w_ih']
        self.enc_w_hh = self.variables['enc_w_hh']
        self.enc_b_ih = self.variables['enc_b_ih']
        self.enc_b_hh = self.variables['enc_b_hh']
        self.dec_emb = self.variables['dec_emb']
        self.dec_w_ih = self.variables['dec_w_ih']
        self.dec_w_hh = self.variables['dec_w_hh']
        self.dec_b_ih = self.variables['dec_b_ih']
        self.dec_b_hh = self.variables['dec_b_hh']
        self.fc_w = self.variables['fc_w']
        self.fc_b = self.variables['fc_b']

    def sigmoid(self, x):
        return 1 / (1 + np.exp(-x))

    def grucell(self, x, h, w_ih, w_hh, b_ih, b_hh):
        rzn_ih = np.matmul(x, w_ih.T) + b_ih
        rzn_hh = np.matmul(h, w_hh.T) + b_hh
        rz_ih, n_ih = (rzn_ih[:, :rzn_ih.shape[-1] * 2 // 3], rzn_ih[:, rzn_ih.shape[-1] * 2 // 3:])
        rz_hh, n_hh = (rzn_hh[:, :rzn_hh.shape[-1] * 2 // 3], rzn_hh[:, rzn_hh.shape[-1] * 2 // 3:])
        rz = self.sigmoid(rz_ih + rz_hh)
        r, z = np.split(rz, 2, -1)
        n = np.tanh(n_ih + r * n_hh)
        h = (1 - z) * n + z * h
        return h

    def gru(self, x, steps, w_ih, w_hh, b_ih, b_hh, h0=None):
        if h0 is None:
            h0 = np.zeros((x.shape[0], w_hh.shape[1]), np.float32)
        h = h0
        outputs = np.zeros((x.shape[0], steps, w_hh.shape[1]), np.float32)
        for t in range(steps):
            h = self.grucell(x[:, t, :], h, w_ih, w_hh, b_ih, b_hh)
            outputs[:, t, :] = h
        return outputs

    def encode(self, word):
        chars = list(word) + ['</s>']
        x = [self.g2idx.get(char, self.g2idx['<unk>']) for char in chars]
        x = np.take(self.enc_emb, np.expand_dims(x, 0), axis=0)
        return x

    def predict(self, word):
        enc = self.encode(word)
        enc = self.gru(enc, len(word) + 1, self.enc_w_ih, self.enc_w_hh, self.enc_b_ih, self.enc_b_hh, h0=np.zeros((1, self.enc_w_hh.shape[-1]), np.float32))
        last_hidden = enc[:, -1, :]
        dec = np.take(self.dec_emb, [2], axis=0)
        h = last_hidden
        preds = []
        for i in range(20):
            h = self.grucell(dec, h, self.dec_w_ih, self.dec_w_hh, self.dec_b_ih, self.dec_b_hh)
            logits = np.matmul(h, self.fc_w.T) + self.fc_b
            pred = logits.argmax()
            if pred == 3:
                break
            preds.append(pred)
            dec = np.take(self.dec_emb, [pred], axis=0)
        preds = [self.idx2p.get(idx, '<unk>') for idx in preds]
        return preds
