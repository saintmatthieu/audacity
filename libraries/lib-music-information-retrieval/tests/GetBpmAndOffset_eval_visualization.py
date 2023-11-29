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

# Store in new variable o2 the values in the octave error column when present.
o2 = df['o2'].dropna()

# Show distribution of octave error
sns.histplot(o2, bins=20)
plt.xlabel('Octave error')
plt.ylabel('Count')
plt.title('Distribution of octave error for true positives')
plt.show()
