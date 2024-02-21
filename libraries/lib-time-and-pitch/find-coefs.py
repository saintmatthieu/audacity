# This script opens a file, `C:/Users/saint/Downloads/roots.txt`, that is a vector `[x1, x2, ..., xN]`. This vector is the roots of a polynomial of order N-1.
# This script finds the coefs of this polynomial and writes them to a file, `C:/Users/saint/Downloads/coefs.txt`.

import numpy as np


roots = []
# Open the file and read the roots
with open('C:/Users/saint/Downloads/rootsOut.txt', 'r') as f:
    # The roots are complex values in the format
    #1.73183 1.17116
    #1.73183 -1.17116
    #-0.279403 0
    #etc.
    for line in f:
        real_part, imag_part = map(float, line.split())
        roots.append(complex(real_part, imag_part))

# Find the coefficients
coefs = np.poly(roots)

# Reverse the coefficients
coefs = coefs[::-1]

# Write the coefficients to a file
with open('C:/Users/saint/Downloads/coefsOut.txt', 'w') as f:
    for coef in coefs:
        f.write(f'{coef}\n')
