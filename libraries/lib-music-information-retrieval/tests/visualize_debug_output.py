from debug_output import *

# Make a plot of the ODF, with peaks marked. Add another layer where the tatums are shown, for example as vertical lines.
import matplotlib.pyplot as plt
import math

# Plot the ODF
plt.plot(odf)
# Plot the peaks
plt.plot(odf_peak_indices, [odf[i] for i in odf_peak_indices], 'ro')
# Convert tatum rate from tatums per minute to tatums per ODF sample:
tatumRate = tatumRate / 60.0 / odfSr

# Plot the tatums as vertical lines, offset by `lag` samples
numTatums = int(math.ceil(len(odf) * tatumRate))
for i in range(numTatums):
    plt.axvline(x=i/tatumRate + lag, color='g', linestyle='--')

plt.title(wavFile)
plt.show()
