using System;
using System.IO;
using System.Reflection;
using System.Threading;
class PriorityResamplerProbe {
    static int Main(string[] args) {
        if (args.Length < 11) return 2;
        string prefix = Assembly.GetExecutingAssembly().Location;
        File.AppendAllText(prefix + ".order", args[2] + "|" + args[4] + "\n");
        if (args[4].Contains("WAIT")) {
            for (int i = 0; i < 1500 && !File.Exists(prefix + ".release"); ++i) Thread.Sleep(10);
            if (!File.Exists(prefix + ".release")) return 3;
        }
        File.Copy(args[0], args[1], true);
        return 0;
    }
}
