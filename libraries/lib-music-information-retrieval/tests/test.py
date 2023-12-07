# The first argument is the name of a csv file with
# truth,score
# header. Produce code to visualize the ROC.
#
# Usage: python3 visualizeROC.py <csv file>
#
# Example: python3 visualizeROC.py ../data/ROC.csv
#

import sys
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Read the CSV file.
csv_path = sys.argv[1]
df = pd.read_csv(csv_path)

# The file has columns truth,score
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
