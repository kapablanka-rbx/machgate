#ifndef _MACHGATE_EXECVE_REEXEC_H_
#define _MACHGATE_EXECVE_REEXEC_H_

int machgate_path_is_macho(const char* path);
int machgate_execve_macho_guest(const char* path, char* const guest_argv[],
                                char* const guest_envp[]);
int machgate_execve_macho_guest_forksafe(const char* path,
                                         char* const guest_argv[],
                                         char* const guest_envp[]);

#endif /* _MACHGATE_EXECVE_REEXEC_H_ */
