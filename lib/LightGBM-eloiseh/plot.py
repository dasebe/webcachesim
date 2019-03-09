import matplotlib.pyplot as plt
import numpy as np
import statistics


def plot_bytes():
    with open("bytes") as fp:
        windows = []
        onces = []
        twices = []
        threes = []
        for line in fp:
            window, once, twice, three = line.split()
            windows.append(int(window))
            onces.append(int(once) / 1073741824)
            twices.append(int(twice) / 1073741824)
            threes.append(int(three) / 1073741824)
        fig, ax = plt.subplots()
        ax.plot(windows, onces, label="once")
        ax.plot(windows, twices, label="twice")
        ax.plot(windows, threes, label="three times")
        ax.legend(loc='upper left')
        plt.grid()
        plt.axis([0, 50, 0, 600])
        plt.xlabel('# Requests (m)')
        plt.ylabel('Total Object Size (GB)')
        plt.title('Total Object Size vs. # Requests')
        plt.show()


def plot_bhr():
    cache_map = {}
    with open("bhr") as fp:
        for line in fp:
            cache, window, ohr, bhr = line.split()
            cache = int(cache) / 1073741824
            if cache not in cache_map:
                cache_map[cache] = {}
            cache_map[cache][int(window) / 1000000] = float(bhr)
    fig, ax = plt.subplots()
    for key, value in sorted(cache_map.items()):
        x, y = zip(*sorted(value.items()))
        ax.plot(x, y, label=str(key) + 'GB')
    ax.legend(loc='upper left')
    plt.grid()
    plt.axis([0, 200, 0.2, 0.8])
    plt.xlabel('Window Size (m)')
    plt.ylabel('PFOO-L BHR')
    plt.title('OPT BHR vs. Window Size')
    plt.show()


def plot_cutoff():
    with open("cutoff") as f:
        cutoffs = []
        fps = []
        fns = []
        errs = []
        for line in f:
            cutoff, fp, fn = line.split()
            cutoffs.append(float(cutoff))
            fps.append(float(fp) * 100)
            fns.append(float(fn) * 100)
            errs.append((float(fp) + float(fn)) * 100)
        fig, ax = plt.subplots()
        ax.plot(cutoffs, fps, label="False Positive (Accidentally Admitted)")
        ax.plot(cutoffs, fns, label="False Negative (Accidentally Not Admitted)", color='red', linestyle='dashed')
        ax.plot(cutoffs, errs, label="False Positive + False Negative", linestyle='-.')
        ax.legend(loc='upper left')
        plt.axis([0, 1, 0, 50])
        plt.grid()
        plt.xlabel('Likelihood Cutoff')
        plt.ylabel('Prediction Error [%]')
        plt.title('Prediction Error vs. Likelihood Cutoff\n(32GB Cache, 20m Window)')
        plt.show()


def plot_prediction():
    with open("prediction") as f:
        windows = []
        fps = []
        fns = []
        errs = []
        for line in f:
            _, window, _, fp, fn = line.split()
            windows.append(int(window) / 1000000)
            fps.append(float(fp) * 100)
            fns.append(float(fn) * 100)
            errs.append((float(fp) + float(fn)) * 100)
        fig, ax = plt.subplots()
        ax.plot(windows, fps, label="False Positive (Accidentally Admitted)")
        ax.plot(windows, fns, label="False Negative (Accidentally Not Admitted)", color='red', linestyle='dashed')
        ax.plot(windows, errs, label="False Positive + False Negative", linestyle='-.')
        ax.legend(loc='upper left')
        plt.axis([0, 50, 0, 50])
        plt.grid()
        plt.xlabel('Window Size (m)')
        plt.ylabel('Prediction Error [%]')
        plt.title('Prediction Error vs. Window Size\n(32GB Cache, 0.5 Cutoff)')
        plt.show()


def plot_cache():
    with open("cache") as f:
        caches = []
        bhrs = []
        for line in f:
            cache, bhr = line.split()
            caches.append(cache)
            bhrs.append(float(bhr))
        fig, ax = plt.subplots()
        ax.barh(caches, bhrs)
        ax.xaxis.grid(linestyle='--')
        ax.invert_yaxis()  # labels read top-to-bottom
        plt.xlabel('Byte Hit Ratio')
        plt.title('BHR vs. Caching Systems\n(32GB Cache)')
        plt.show()


def plot_time():
    with open("time") as f:
        times = {}
        for line in f:
            event, time = line.split()
            if event in times:
                times[event] += int(time)
            else:
                times[event] = int(time)
    times['Other'] += times['ProcessWindow'] - (times['CalculateOPT'] + times['DeriveFeatures'] + times['TrainModel'])
    times.pop('ProcessWindow')
    plt.pie(times.values(), labels=times.keys(), autopct='%1.1f%%', startangle=90)
    plt.title('Runtime Distribution\n(32GB Cache, 20m Window, 200m Trace)')
    plt.show()


def plot_model():
    data = {}
    with open("model") as f:
        for line in f:
            parts = line.split()
            if len(parts) == 6:
                key = parts[0]
                fp = parts[4]
                fn = parts[5]
                if key not in data:
                    data[key] = [[], []]
                data[key][0].append(float(fp) * 100)
                data[key][1].append(float(fn) * 100)
    fig, ax = plt.subplots()
    width = 0.3
    for key in data.keys():
        ax.bar(np.arange(9), data[key][0], align='edge', width=width, label=key+' (False Positive)')
        ax.bar(np.arange(9), data[key][1], align='edge', width=width, bottom=data[key][0], label=key+' (False Negative)')
        width = -0.3
    ax.legend(loc='upper left')
    plt.xlabel('Window')
    plt.ylabel('Prediction Error [%]')
    plt.xticks(np.arange(0, 9, 1))
    plt.yticks(np.arange(0, 20, 2))
    plt.title('Prediction Error vs. Window\n(32GB Cache, 0.5 Cutoff)')
    plt.show()


def plot_sampling():
    with open("sampling") as f:
        samples = []
        fps = []
        fns = []
        errs = []
        for line in f:
            _, _, sample, _, fp, fn, _, _ = line.split()
            samples.append(int(sample) / 1000000)
            fps.append(float(fp) * 100)
            fns.append(float(fn) * 100)
            errs.append((float(fp) + float(fn)) * 100)
        fig, ax = plt.subplots()
        ax.plot(samples, fps, label="False Positive (Accidentally Admitted)")
        ax.plot(samples, fns, label="False Negative (Accidentally Not Admitted)", color='red', linestyle='dashed')
        ax.plot(samples, errs, label="False Positive + False Negative", linestyle='-.')
        ax.legend(loc='upper left')
        plt.axis([0, 20, 0, 50])
        plt.grid()
        plt.xlabel('Most Recent Sample Size (m)')
        plt.ylabel('Prediction Error [%]')
        plt.title('Prediction Error vs. Most Recent Sample Size\n(32GB Cache, 20m Window, 0.5 Cutoff)')
        plt.show()


def plot_randomsampling():
    with open("randomsampling") as f:
        data = {}
        for line in f:
            _, _, sample, _, _, fp, fn, _, _ = line.split()
            if sample not in data:
                data[sample] = [[], [], []]
            data[sample][0].append(float(fp) * 100)
            data[sample][1].append(float(fn) * 100)
            data[sample][2].append((float(fp) + float(fn)) * 100)
        keys = []
        fps = []
        fpstds = []
        fns = []
        fnstds = []
        errs = []
        errstds = []
        for key in data.keys():
            keys.append(int(key)/1000000)
            fps.append(statistics.mean(data[key][0]))
            fpstds.append(statistics.stdev(data[key][0]))
            fns.append(statistics.mean(data[key][1]))
            fnstds.append(statistics.stdev(data[key][1]))
            errs.append(statistics.mean(data[key][2]))
            errstds.append(statistics.stdev(data[key][2]))
        fig, ax = plt.subplots()
        ax.errorbar(keys, fps, yerr=fpstds, label="False Positive (Accidentally Admitted)")
        ax.errorbar(keys, fns, yerr=fnstds, label="False Negative (Accidentally Not Admitted)", color='red', linestyle='dashed')
        ax.errorbar(keys, errs, yerr=errstds, label="False Positive + False Negative", linestyle='-.')
        ax.legend(loc='upper left')
        plt.axis([0, 20, 0, 20])
        plt.grid()
        plt.xlabel('Random Sample Size (m)')
        plt.ylabel('Prediction Error [%]')
        plt.title('Prediction Error vs. Random Sample Size\n(32GB Cache, 20m Window, 0.5 Cutoff)')
        plt.show()


# plot_cutoff()
# plot_bytes()
# plot_bhr()
# plot_prediction()
# plot_cache()
# plot_time()
# plot_model()
# plot_sampling()
plot_randomsampling()
