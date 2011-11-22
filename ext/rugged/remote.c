/*
 * The MIT License
 *
 * Copyright (c) 2011 GitHub, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "rugged.h"

extern VALUE rb_mRugged;
extern VALUE rb_cRuggedRepo;
VALUE rb_cRuggedRemote;

static void rb_git_remote__mark(rugged_remote *remote)
{
	rb_gc_mark(remote->owner);
}

static void rb_git_remote__free(rugged_remote *remote)
{
	git_remote_free(remote->remote);
	free(remote);
}

static VALUE rb_git_remote__new(int argc, VALUE *argv, VALUE klass)
{
	VALUE rb_repo, rb_url, rb_name;
	rugged_remote *remote;
	rugged_repository *repo;
	const char *url;
	int error;

	rb_scan_args(argc, argv, "21", &rb_repo, &rb_url, &rb_name);

	Check_Type(rb_url, T_STRING);

	if (!rb_obj_is_instance_of(rb_repo, rb_cRuggedRepo))
		rb_raise(rb_eTypeError, "Expecting a Rugged Repository");

	Data_Get_Struct(rb_repo, rugged_repository, repo);

	url = StringValueCStr(rb_url);

	remote = malloc(sizeof(rugged_remote));
	remote->owner = rb_repo;

	if (git_transport_valid_url(url)) {
		if (!NIL_P(rb_name))
			Check_Type(rb_name, T_STRING);

		error = git_remote_new(
			&remote->remote,
			repo->repo,
			StringValueCStr(rb_url),
			NIL_P(rb_name) ? NULL : StringValueCStr(rb_name)
		);
	} else {
		error = GIT_ENOTIMPLEMENTED;
		/*
		error = git_remote_get(
			&remote->remote,
			git_repository_config(repo->repo),
			url
		);
		*/
	}

	rugged_exception_check(error);

	return Data_Wrap_Struct(klass, &rb_git_remote__mark, &rb_git_remote__free, remote);
}

static VALUE rb_git_remote_disconnect(VALUE self)
{
	rugged_remote *remote;
	Data_Get_Struct(self, rugged_remote, remote);

	git_remote_disconnect(remote->remote);
	return Qnil;
}

static VALUE rb_git_remote_connect(VALUE self, VALUE rb_direction)
{
	int error, direction = 0;
	rugged_remote *remote;
	ID id_direction;

	Data_Get_Struct(self, rugged_remote, remote);

	Check_Type(rb_direction, T_SYMBOL);
	id_direction = SYM2ID(rb_direction);

	if (id_direction == rb_intern("fetch"))
		direction = GIT_DIR_FETCH;
	else if (id_direction == rb_intern("push"))
		direction = GIT_DIR_PUSH;
	else
		rb_raise(rb_eTypeError,
			"Invalid remote direction. Expected `:fetch` or `:push`");

	error = git_remote_connect(remote->remote, direction);
	rugged_exception_check(error);

	if (rb_block_given_p())
		rb_ensure(rb_yield, self, rb_git_remote_disconnect, self);

	return Qnil;
}

static VALUE rb_git_remote_name(VALUE self)
{
	rugged_remote *remote;
	Data_Get_Struct(self, rugged_remote, remote);

	return rugged_str_new2(git_remote_name(remote->remote), NULL);
}

static VALUE rb_git_remote_connected(VALUE self)
{
	rugged_remote *remote;
	Data_Get_Struct(self, rugged_remote, remote);

	return git_remote_connected(remote->remote) ? Qtrue : Qfalse;
}

void Init_rugged_remote()
{
	rb_cRuggedRemote = rb_define_class_under(rb_mRugged, "Remote", rb_cObject);

	rb_define_singleton_method(rb_cRuggedRemote, "new", rb_git_remote__new, -1);

	rb_define_method(rb_cRuggedRemote, "connect", rb_git_remote_connect, 1);
	rb_define_method(rb_cRuggedRemote, "disconnect", rb_git_remote_disconnect, 0);
	rb_define_method(rb_cRuggedRemote, "name", rb_git_remote_name, 0);
	rb_define_method(rb_cRuggedRemote, "connected?", rb_git_remote_connected, 0);
}