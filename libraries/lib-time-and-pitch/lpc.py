# Read audio file "C:/Users/saint/Downloads/ah-windowed.wav"

import numpy as np
import matplotlib.pyplot as plt
import scipy.io.wavfile as wav
import scipy.signal
import librosa

# Read the audio file
rate, y = wav.read("C:/Users/saint/Downloads/ah-windowed.wav")
# convert y to float
y = y.astype(float)

# Run an LPC analysis on the data
a = librosa.lpc(y, order=3)
b = np.hstack([[0], -1 * a[1:]])
y_hat = scipy.signal.lfilter(b, [1], y)
fig, ax = plt.subplots()
ax.plot(y)
ax.plot(y_hat, linestyle='--')
ax.legend(['y', 'y_hat'])
ax.set_title('LP Model Forward Prediction')

# show the transfer function of coefficients `a` on the z-plane
z, p, k = scipy.signal.tf2zpk([1], a)
fig, ax = plt.subplots()
ax.scatter(np.real(z), np.imag(z), marker='o', color='b')
ax.scatter(np.real(p), np.imag(p), marker='x', color='r')
ax.set_title('Pole-Zero Plot')

plt.show()
