using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace DxxRegression
{
    // https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects
    public static class ProcessLifetime
    {
        // Deliberately held until process exit, not disposed at script return
        // The unnamed handle is non-inheritable; descendants inherit membership only
        private static IntPtr job;

        [StructLayout(LayoutKind.Sequential)]
        private struct BasicLimits
        {
            public long ProcessTime, JobTime;
            public uint Flags;
            public UIntPtr MinimumWorkingSet, MaximumWorkingSet;
            public uint ActiveProcesses;
            public UIntPtr Affinity;
            public uint PriorityClass, SchedulingClass;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct ExtendedLimits
        {
            public BasicLimits Basic;
            public ulong ReadOperations, WriteOperations, OtherOperations;
            public ulong ReadBytes, WriteBytes, OtherBytes;
            public UIntPtr ProcessMemory, JobMemory, PeakProcessMemory, PeakJobMemory;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateJobObject(IntPtr attributes, string name);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetInformationJobObject(IntPtr job, int kind, ref ExtendedLimits limits, uint size);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
        [DllImport("kernel32.dll")]
        private static extern bool CloseHandle(IntPtr handle);

        public static void Initialize()
        {
            if (job != IntPtr.Zero) return;
            IntPtr created = CreateJobObject(IntPtr.Zero, null);
            if (created == IntPtr.Zero) throw new Win32Exception();
            try
            {
                var limits = new ExtendedLimits();
                limits.Basic.Flags = 0x2000; // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
                if (!SetInformationJobObject(created, 9, ref limits,
                    (uint)Marshal.SizeOf(typeof(ExtendedLimits)))) throw new Win32Exception();
                using (var current = Process.GetCurrentProcess())
                    if (!AssignProcessToJobObject(created, current.Handle)) throw new Win32Exception();
                job = created;
            }
            finally
            {
                if (job == IntPtr.Zero) CloseHandle(created);
            }
        }
    }
}
