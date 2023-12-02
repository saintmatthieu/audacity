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

# The file has columns truth,normalizedAutocorrCurvatureRms,beatFittingErrorRms,filename
# We want a scatter plot showing `normalizedAutocorrCurvatureRms` vs `beatFittingErrorRms`,
# with the samples colored by `truth`.
# Additionally, hovering over a sample should show the filename.

# Create a figure.
fig = plt.figure(figsize=(8, 8))

# In the following plot, samples with invalidBeatFittingErrorRmsValue need a special color to be recognized better.
# Create that scatter plot.
sns.scatterplot(
    data=df,
    x="beatSnr",
    y="beatFittingErrorRms",
    hue="truth",
    size="truth",
    sizes=(50, 200),
    palette="deep",
    legend="full",
)

# Add a title.
plt.title("Beat tracking accuracy")

# Add a grid.
plt.grid(True)

# Add a legend.
plt.legend(loc="lower right")

# Show the plot.
plt.show()
