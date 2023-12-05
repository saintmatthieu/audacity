# User calls this script with one argument: the path to a CSV file.
# The CSV contains something like this:
#
# filename,expected,actual
# Big_Band_Drums_-_120BPM_-_Brushes_-_Short_Fill_4.wav,120,154.707,
# Big_Band_Drums_-_120BPM_-_Old_School_-_Shuffle_Backbeat.wav,120,119.132,0.1
# ...
#
# The last column is optional and contains the octave error.

import sys
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Read the CSV file.
csv_path = sys.argv[1]
df = pd.read_csv(csv_path)

# The file has columns truth,score,filename
# Given a threshold T, classify samples whose score is less than T as positive-by-measurement, and calculate the true positive rate (TPR) and false positive rate (FPR).
# Repeat for many values of T, and plot the ROC curve.
# The ROC curve is the plot of TPR vs FPR.
# The area under the ROC curve (AUC) is a measure of the quality of the classifier.
# AUC=1 is perfect, AUC=0.5 is random guessing, AUC=0 is the worst possible classifier.

# Calculate the TPR and FPR for each threshold.
# Sort the dataframe by score.
df = df.sort_values(by='score')
# Calculate the TPR and FPR.
TPR = []
FPR = []
for threshold in df['score']:
    # Count the number of true positives and false positives.
    TP = 0
    FP = 0
    for i in range(len(df)):
        if df['score'][i] < threshold:
            # Positive-by-measurement.
            if df['truth'][i] == 1:
                # Positive-by-truth.
                TP += 1
            else:
                # Negative-by-truth.
                FP += 1
    # Calculate the TPR and FPR.
    TPR.append(TP / sum(df['truth']))
    FPR.append(FP / (len(df) - sum(df['truth'])))

# Plot the ROC curve.
plt.plot(FPR, TPR)
plt.xlabel('False positive rate')
plt.ylabel('True positive rate')
plt.title('ROC curve')
plt.show()
