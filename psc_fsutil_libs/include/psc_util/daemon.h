/* a daemon startup routine, by Richard Stevens
 * sever all ties to the launching process
 */
void daemon_start(void);

#define PIDFILE_DIR "/var/run"

/* write a PID file in PIDFILE_DIR
 * name will be PIDFILE_DIR/dname-id
 * or... without the "-id" if 0==id 
 * Contents will be the running daemon's PID value */
void write_pidfile(const char *dname, int id);
