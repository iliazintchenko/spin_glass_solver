# This script performs some evaluation on the data and makes a plot:
# Histogram of success probability to find exact ground state
# USE ONLY FOR TEST OF WORKFLOW!

import matplotlib.pyplot as plt
import numpy as np

def LoadSaData(filename):
    """ param filename: filename with Energy and Spin Configuration
        return: list of SA ground state energy
    """
    result=[]
    with open(filename) as f:
        for line in f:
            if line[0] == '#':
                continue
            temp=line.split()
            result.append(int(temp[0]))
    return result

def GetExactGroundState(filename):
    """ param filename: filename where exact ground state energy is saved
        return: list of exact ground state energy
    """
    exact_energies=[]
    with open(filename) as f:
        next(f) #skip header
        for line in f:
            temp = line.split()
            exact_energies.append(int(temp[1]))
    return exact_energies

def ReadTestData():
    """ Computes success probability for each instance

        return: list of success probabilities
    """
    
    exact_ground_states = GetExactGroundState(
        "../testdata/Instances128Spins/GroundStateEnergy128Spins.txt")
    
    all_success_prob = []
    
    num_rep=len(LoadSaData("../testdata/Instances128Spins/results/128random0.dat"))
                
    for ii in range(num_rep):
        success = 0
        achieved_energies = LoadSaData("../testdata/Instances128Spins/results/128random"+
                                       str(ii)+".dat")
        for e in achieved_energies:
            if e == exact_ground_states[ii]:
                success += 1
        all_success_prob.append(float(success)/float(num_rep))
                
    return all_success_prob

def main():
    data = ReadTestData()
    fig = plt.figure()
    ax = plt.subplot(1,1,1)
    ax.hist(data)
    ax.set_yscale('log')
    ax.set_ylabel('number of instances')
    ax.set_xlabel('success probability to find ground state')
    fig.savefig('testfigure.pdf')
    plt.show()

if __name__ == "__main__":
    main()
#    print "hello"

