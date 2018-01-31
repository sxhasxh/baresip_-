/**
 * @file stdio.c Standard Input/Output UI module
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <re.h>
#include <baresip.h>


/**
 * @defgroup stdio stdio
 *
 * User-Interface (UI) module for standard input/output
 *
 * This module sets up the terminal in raw mode, and reads characters from the
 * input to the UI subsystem. The module is indented for Unix-based systems.
 */


/** Local constants */
enum {
	RELEASE_VAL = 250  /**< Key release value in [ms] */
};

struct ui_st {
	struct tmr tmr;
	struct termios term;
	bool term_set;
};


/* We only allow one instance */
static struct ui_st *ui_state;


static void ui_destructor(void *arg)
{
	struct ui_st *st = arg;

	fd_close(STDIN_FILENO);

	if (st->term_set)
		tcsetattr(STDIN_FILENO, TCSANOW, &st->term);

	tmr_cancel(&st->tmr);
}

//将p处的size个数据输出到标准错误
static int print_handler(const char *p, size_t size, void *arg)
{
	(void)arg;

	return 1 == fwrite(p, size, 1, stderr) ? 0 : ENOMEM;
}


static void report_key(struct ui_st *ui, char key)
{
	static struct re_printf pf_stderr = {print_handler, NULL};
	(void)ui;

	ui_input_key(baresip_uis(), key, &pf_stderr);
}


static void timeout(void *arg)
{
	struct ui_st *st = arg;

	/* Emulate key-release */
	report_key(st, KEYCODE_REL);
}


static void ui_fd_handler(int flags, void *arg)
{
	struct ui_st *st = arg;
	char key;
	(void)flags;

	if (1 != read(STDIN_FILENO, &key, 1)) {
		return;
	}

	tmr_start(&st->tmr, RELEASE_VAL, timeout, st);
	report_key(st, key);
}


static int term_setup(struct ui_st *st)
{
	struct termios now;

	if (tcgetattr(STDIN_FILENO, &st->term) < 0)//获取标准输入当前属性
		return errno;

	now = st->term;

	now.c_lflag |= ISIG;
	now.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);

	/* required on Solaris */
	now.c_cc[VMIN] = 1;
	now.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &now) < 0)//设置属性
		return errno;

	st->term_set = true;

	return 0;
}


static int ui_alloc(struct ui_st **stp)
{
	struct ui_st *st;
	int err;

	if (!stp)  //如果非空指针，返回
		return EINVAL;

	st = mem_zalloc(sizeof(*st), ui_destructor);//分配空间，并加入销毁列表
	if (!st)
		return ENOMEM;

	tmr_init(&st->tmr);//初始化tmr结构体
//将标准输入加入监听队列,当EPOOL检查到描述符可用时，就会调用ui_fd_handler,
	err = fd_listen(STDIN_FILENO, FD_READ, ui_fd_handler, st);
	if (err)
		goto out;

	err = term_setup(st);//设置标准输入的属性
	if (err) {
		info("stdio: could not setup terminal: %m\n", err);
		err = 0;
	}

 out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;
}
//此函数将要输出的数据输出到标准错误上，从而在屏幕上显示，标准错误为：无缓冲输出，
//不需要换行符，将要输出的内容立即输出，标准输出为：行缓冲，需要换行才能输出
static int output_handler(const char *str)
{
	return print_handler(str, str_len(str), NULL);
}


static struct ui ui_stdio = {
	.name = "stdio",
	.outputh = output_handler
};


static int module_init(void)
{
	int err;

	err = ui_alloc(&ui_state);
	if (err)
		return err;

	ui_register(baresip_uis(), &ui_stdio);

	return 0;
}


static int module_close(void)
{
	ui_unregister(&ui_stdio);
	ui_state = mem_deref(ui_state);

	return 0;
}


const struct mod_export DECL_EXPORTS(stdio) = {
	"stdio",
	"ui",
	module_init,
	module_close
};
