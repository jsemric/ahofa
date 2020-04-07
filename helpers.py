def export_frequency(freq, filepath, train_file):
    with open(train_file, 'r') as f:
        string_count = int(f.readline())
    with open(filepath, 'w') as f:
        for state, frequency in freq.items():
            significancy = frequency / string_count
            f.write('{}:{}\n'.format(state, significancy))
