# User calls this script with one argument: the path to a CSV file.
# The CSV contains something like this:
#
# filename,expected,actual
# Big_Band_Drums_-_120BPM_-_Brushes_-_Short_Fill_4.wav,120,154.707
# Big_Band_Drums_-_120BPM_-_Old_School_-_Shuffle_Backbeat.wav,120,119.132
# ...
#
# We want to show the distribution of the error for true positives.
# We also display the true positive rate (TPR) and false positive rate (FPR).

import sys
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np

# Read the CSV file.
csv_path = sys.argv[1]
df = pd.read_csv(csv_path)

# Show distribution of error for true positives, i.e, where expected != 0 and actual != 0.
df_true_positives = df[(df['expected'] != 0) & (df['actual'] != 0)]
# Make a vector of the so-called octave-error out of `df_true_positives`, i.e, where log2(y_hat / y).
octave_error = np.log2(df_true_positives['actual'] / df_true_positives['expected'])
# Errors by a factor of 2 and 1/2 are actually considered correct.
octave_error_double = np.log2(df_true_positives['actual'] / df_true_positives['expected'] * 2)
octave_error_half = np.log2(df_true_positives['actual'] / df_true_positives['expected'] / 2)
# Transform octave_error such that it that with least absolute value of the three.
octave_error = np.where(np.abs(octave_error_double) < np.abs(octave_error), octave_error_double, octave_error)
octave_error = np.where(np.abs(octave_error_half) < np.abs(octave_error), octave_error_half, octave_error)

# Show distribution of octave error
sns.histplot(octave_error, bins=20)
plt.xlabel('Octave error')
plt.ylabel('Count')
plt.title('Distribution of octave error for true positives')
plt.show()
