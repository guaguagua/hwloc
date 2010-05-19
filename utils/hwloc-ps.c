/*
 * Copyright © 2009 CNRS, INRIA, Université Bordeaux 1
 * Copyright © 2009 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

#include <private/config.h>
#include <hwloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>

static void usage(char *name, FILE *where)
{
  fprintf (where, "Usage: %s [ options ] ... [ filename ]\n\n", name);
  fprintf (where, "   -a       Show all processes, including those that are not bound\n");
}

int main(int argc, char *argv[])
{
  const struct hwloc_topology_support *support;
  hwloc_topology_t topology;
  hwloc_obj_t root;
  hwloc_cpuset_t cpuset;
  DIR *dir;
  struct dirent *dirent;
  int show_all = 0;
  char *callname;
  int err;
  char c;
  int opt;

  callname = strrchr(argv[0], '/');
  if (!callname)
    callname = argv[0];
  else
    callname++;

  while (argc >= 2) {
    opt = 0;
    if (!strcmp(argv[1], "-a"))
      show_all = 1;
    else {
      fprintf (stderr, "Unrecognized options: %s\n", argv[1]);
      usage (callname, stderr);
      exit(EXIT_FAILURE);
    }
    argc -= opt+1;
    argv += opt+1;
  }

  err = hwloc_topology_init(&topology);
  if (err)
    goto out;

  err = hwloc_topology_load(topology);
  if (err)
    goto out_with_topology;

  root = hwloc_get_root_obj(topology);

  support = hwloc_topology_get_support(topology);

  if (!support->cpubind->get_thisproc_cpubind)
    goto out_with_topology;

  dir  = opendir("/proc");
  if (!dir)
    goto out_with_topology;

  cpuset = hwloc_cpuset_alloc();
  if (!cpuset)
    goto out_with_dir;

  while ((dirent = readdir(dir))) {
    long pid;
    char *end;
    char name[64] = "";
    char *cpuset_str = NULL;

    pid = strtol(dirent->d_name, &end, 10);
    if (*end)
      /* Not a number */
      continue;

#ifdef HWLOC_LINUX_SYS
    {
      char path[6 + strlen(dirent->d_name) + 1 + 7 + 1];
      int file;
      ssize_t n;

      snprintf(path, sizeof(path), "/proc/%s/cmdline", dirent->d_name);

      if ((file = open(path, O_RDONLY)) >= 0) {
        n = read(file, name, sizeof(name) - 1);
        close(file);

        if (n <= 0)
          /* Ignore kernel threads and errors */
          continue;
      }
      name[n] = 0;
    }
#endif /* HWLOC_LINUX_SYS */

    if (hwloc_get_proc_cpubind(topology, pid, cpuset, 0))
      continue;

    hwloc_cpuset_asprintf(&cpuset_str, cpuset);
    if (!cpuset_str)
      continue;

    if (hwloc_cpuset_isequal(cpuset, root->cpuset) && !show_all)
      continue;

    printf("%ld\t%s\t%s\n", pid, cpuset_str, name);
    free(cpuset_str);
  }

  err = 0;
  hwloc_cpuset_free(cpuset);

 out_with_dir:
  closedir(dir);
 out_with_topology:
  hwloc_topology_destroy(topology);
 out:
  return err;
}
