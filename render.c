
#include "slider.h"

void *render_threaded(void *arg) {
	Show *show = (Show *) arg;
	int i, j, n, x, y, grid = 0;
	int sw = show->w, sh = show->h;
	Pixmap thumb = 0x0;
	/* open pdf and create Show */
	PopplerDocument *pdf = poppler_document_new_from_file(show->uri,NULL,NULL);
	if (!pdf) die("\"%s\" is not a pdf file\n",show->uri);
	show->count = poppler_document_get_n_pages(pdf);
	show->slide = (Pixmap *) calloc(show->count, sizeof(Pixmap));
	show->flag = (int *) calloc(show->count, sizeof(int));
	/* scaling calculations */
	double pdfw, pdfh;
	PopplerPage *page = poppler_document_get_page(pdf,0);
	poppler_page_get_size(page,&pdfw,&pdfh);
	float hsc = show->w/pdfw, vsc = show->h/pdfh;
	if (hsc > vsc) {
		show->scale = vsc;
		show->w = pdfw * show->scale;
		show->x = (sw - show->w)/2;
	}
	else {
		show->scale = hsc;
		show->h = pdfh * show->scale;
		show->y = (sh - show->h)/2;
	}
	/* create sorter */
	if (show->sorter) {
		/* scaling calculations sorter */
		show->sorter->count = 1;
		show->sorter->slide = (Pixmap *) calloc(1, sizeof(Pixmap));
		show->sorter->flag = (int *) calloc(1,sizeof(int));
		grid = (int) ceil(sqrt(show->count)) ;
		vsc = ( (sh-10)/grid - 10) / pdfh;
		hsc = ( (sw-10)/grid - 10) / pdfw;
		show->sorter->flag[0] = grid;
		show->sorter->scale = (vsc > hsc ? hsc : vsc);
		show->sorter->h = pdfh * show->sorter->scale;
		show->sorter->w = pdfw * show->sorter->scale;
		show->sorter->x = (sw - (show->sorter->w+10)*grid)/2;
		show->sorter->y = (sh - (show->sorter->h+10)*grid)/2;
		/* create empty sorter frame */
		thumb = XCreatePixmap(dpy,root,show->sorter->w,
				show->sorter->h,DefaultDepth(dpy,scr));
		show->sorter->slide[0] = XCreatePixmap(dpy,root,sw,sh,
				DefaultDepth(dpy,scr));
		XFillRectangle(dpy,show->sorter->slide[0],cgc(ScreenBG),0,0,sw,sh);
		cgc(SlideBG);
		n = 0; y = 10;
		for (i = 0; i < grid; i++, y += show->sorter->h + 10) {
			x = show->sorter->x;
			for (j = 0; j < grid; j++, x+= show->sorter->w + 10) {
				if (++n > show->count) break;
				XFillRectangle(dpy,show->sorter->slide[0],cgc(Empty),x+2,y+2,
						show->sorter->w-5,show->sorter->h-5);
			}
		}
	}	
	/* render pages */
	cairo_surface_t *target;
	cairo_t *cairo;
	n = 0; x = (show->sorter ? (sw-grid*(show->sorter->w+10))/2 : 0); y = 10;
	for (i = 0; i < show->count; i++) {
		show->slide[i] = XCreatePixmap(dpy,root,show->w,show->h,
				DefaultDepth(dpy,scr));
		XFillRectangle(dpy,show->slide[i],cgc(SlideBG),0,0,show->w,show->h);
		page = poppler_document_get_page(pdf,i);
		target = cairo_xlib_surface_create(dpy,show->slide[i],
				DefaultVisual(dpy,scr),show->w,show->h);
		cairo = cairo_create(target);
		cairo_scale(cairo,show->scale,show->scale);
		poppler_page_render(page,cairo);
		cairo_surface_destroy(target);
		cairo_destroy(cairo);
		show->flag[i] |= RENDERED;
		if (show->sorter) {
			XFillRectangle(dpy,thumb,cgc(SlideBG),0,0,
					show->sorter->w,show->sorter->h);
			target = cairo_xlib_surface_create(dpy,thumb,DefaultVisual(dpy,scr),
					show->sorter->w,show->sorter->h);
			cairo = cairo_create(target);
			cairo_scale(cairo,show->sorter->scale,show->sorter->scale);
			poppler_page_render(page,cairo);
			cairo_surface_destroy(target);
			XCopyArea(dpy,thumb,show->sorter->slide[0],gc,0,0,show->sorter->w,
					show->sorter->h,x,y);
			x += show->sorter->w + 10;
			if (++n == grid) {
				n = 0;
				x = (sw-grid*(show->sorter->w+10))/2;
				y += show->sorter->h + 10;
			}
		}
	}
	if (show->sorter) XFreePixmap(dpy,thumb);
	return NULL;
}


void render(Show *show) {
	pthread_t show_render, note_render;
	pthread_create(&show_render,NULL,&render_threaded,(void *) show);
	while (!(show->flag)) usleep(5000);
	if (prerender == 0 || prerender >= show->count) prerender = show->count - 1;
	while (!(show->flag[(prerender>0?prerender:1)] & RENDERED)) usleep(50000);

	if (show->notes && show->notes->uri) {
		pthread_create(&note_render,NULL,render_threaded,(void *) show->notes);
		while (!(show->notes->flag)) usleep(5000);
		while (!(show->notes->flag[1] & RENDERED)) usleep(50000);
	}
}

void free_renderings(Show *show) {
	if (!show) return;
	int i;
	for (i = 0; i < show->count; i++) {
		XFreePixmap(dpy,show->slide[i]);
	}
	free(show->slide);
	free(show->flag);
	if (show->sorter) {
		XFreePixmap(dpy,show->sorter->slide[0]);
		free(show->sorter->slide);
		free(show->sorter->flag);
	}
	if (show->notes) {
		for (i = 0; i < show->notes->count; i++)
			XFreePixmap(dpy,show->notes->slide[i]);
		free(show->notes->slide);
		free(show->notes->flag);
	}
}

void die(const char *msg,...) {
	va_list arg;
	va_start(arg,msg);
	vfprintf(stderr,msg,arg);
	va_end(arg);
	exit(1);
}

