def export_frequency(freq, filepath):
    f = open(filepath, 'w')
    for state, frequency in freq.items():
        f.write('{}:{}\n'.format(state, frequency))
    f.close()
