# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgsthivosink.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgsthivosink_la_SOURCES = gstvideosink.c gstvideosink.h gst_hisi_vo_context.c gst_hisi_vo_context.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgsthivosink_la_CFLAGS = $(GST_CFLAGS)
libgsthivosink_la_LIBADD = $(GST_LIBS)
libgsthivosink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -lhi_vou -lhi_common -lgstvideo-1.0
libgsthivosink_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gst_hisi_vo_context.h gstvideosink.h
