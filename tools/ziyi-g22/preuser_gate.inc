	/* Explicit RAM-only diagnostic gate: before autoconfiguration/root mount.
	 * The helper returns immediately in every ordinary iOS boot. */
	extern int ziyi_gpu_preuser_start(void);
	(void) ziyi_gpu_preuser_start();

