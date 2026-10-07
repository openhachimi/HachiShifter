using System;
using System.IO;
using System.Reflection;
class ResamplerProbe {
    static int Main(string[] args) {
        File.WriteAllLines(Assembly.GetExecutingAssembly().Location + ".args.txt", args);
        if (args.Length < 11) return 2;
        File.Copy(args[0], args[1], true);
        return 0;
    }
}