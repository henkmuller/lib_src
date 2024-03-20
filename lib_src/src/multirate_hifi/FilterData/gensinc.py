import numpy as np

def gensinc(N, fc, name):
    N += 2 # zero on either side will be thrown off
    h = np.zeros((N))
    for n in range(N):
        val = np.sinc(2*fc*(n-(N-1)/2)) *(0.42-0.5*np.cos(2*np.pi*n/(N-1)) + 0.08*np.cos(4*np.pi*n/(N-1)))
        h[n] = val
    summie = np.sum(h)
    h = h / summie * 1024*1024*1024 * 4
    outp = ''
    h = h[1:N-1]   # remove zeroes at the end
    for i in h:
        outp += str(int(i))+',\n'
    with open(name + '.dat', 'w') as fil:
        fil.write(outp)
    
    outp = ''
    for i in range(0, len(h), 16):
        for k in range(2):
            for j in range(8):
                outp += str(int(h[i+k+j*2]))+',\n'
    with open(name + '_xs3.dat', 'w') as fil:
        fil.write(outp)

h = gensinc(160, 16000/48000/2, 'UP4832')
h = gensinc(160, 12000/32000/2, 'UP3224')
