# This script opens a file, `C:/Users/saint/Downloads/coefs.txt`, that is a vector `[x1, x2, ..., xN]`. This vector is the coefficients of a polynomial of order N-1, in ascending order of degree.
# This script finds the roots of the polynomial and writes them to a file, `C:/Users/saint/Downloads/roots.txt`.

import numpy as np

# Open the file and read the coefficients
with open('C:/Users/saint/Downloads/coefsIn.txt', 'r') as f:
    coefs = np.array([float(x) for x in f.read().split()])
    N = len(coefs)

# Find the roots
roots = np.roots(coefs)

# Write the roots to a file in format
# <real1>
# <imag1>
# <real2>
# <imag2>
# ...
with open('C:/Users/saint/Downloads/rootsIn.txt', 'w') as f:
    for root in roots:
        f.write(f'{root.real} {root.imag}\n')
