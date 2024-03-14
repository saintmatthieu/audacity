from debug_output import *

# Make a plot of the ODF, with peaks marked. Add another layer where the tatums are shown, for example as vertical lines.
import matplotlib.pyplot as plt
import math

wavFileStem = wavFile.split('/')[-1].split('.')[0]

# Plot `rawOdf` and `movingAverage` on the same plot.
plt.figure(1)
plt.plot([i/odfSr for i in range(len(odf))], rawOdf)
# Now use the ODF indices in `cluster1`, `cluster2`, and `cluster3`, to highlight the peaks with different colors
plt.plot([rawOdfPeakIndices[i]/odfSr for i in cluster1], [rawOdf[rawOdfPeakIndices[i]] for i in cluster1], 'rx' if 0 in suppressedClusters else 'ro')
plt.plot([rawOdfPeakIndices[i]/odfSr for i in cluster2], [rawOdf[rawOdfPeakIndices[i]] for i in cluster2], 'bx' if 1 in suppressedClusters else 'bo')
plt.plot([rawOdfPeakIndices[i]/odfSr for i in cluster3], [rawOdf[rawOdfPeakIndices[i]] for i in cluster3], 'gx' if 2 in suppressedClusters else 'go')
plt.xlim(0, len(odf) / odfSr)
plt.grid(True)
plt.title("{stem}: ODF before peak suppression".format(stem=wavFileStem))

# Plot `odfAutoCorr`, whose length is half that of `odf` + 1. Add a layer where the peaks at `odfAutoCorrPeakIndices` are marked.
plt.figure(2)
plt.plot([i/odfSr for i in range(len(odfAutoCorr))], odfAutoCorr)
plt.plot([i/odfSr for i in odfAutoCorrPeakIndices], [odfAutoCorr[i] for i in odfAutoCorrPeakIndices], 'ro')
plt.xlim(0, len(odfAutoCorr) / odfSr)
plt.grid(True)
plt.title("{stem}: Auto-correlation of ODF".format(stem=wavFileStem))

plt.figure(3)
# Plot the ODF, using odfSr to convert from samples to seconds
plt.plot([i/odfSr for i in range(len(odf))], odf)
# Plot the peaks
plt.plot([i/odfSr for i in odf_peak_indices], [odf[i] for i in odf_peak_indices], 'ro')
tatumsPerSecond = tatumRate / 60.0

# Plot the tatums as vertical lines, offset by `lag` samples
numTatums = int(math.ceil(len(odf) / odfSr * tatumsPerSecond))
for i in range(numTatums):
    plt.axvline(x=i/tatumsPerSecond + lag / odfSr, color='g', linestyle='--')

plt.xlim(0, len(odf) / odfSr)
plt.title("{stem}: Quantization fit".format(stem=wavFileStem))

plt.show()
