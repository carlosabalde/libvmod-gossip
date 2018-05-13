
.. image:: https://travis-ci.org/carlosabalde/libvmod-gossip.svg?branch=6.0
   :alt: Travis CI badge
   :target: https://travis-ci.org/carlosabalde/libvmod-gossip/
.. image:: https://codecov.io/gh/carlosabalde/libvmod-gossip/branch/6.0/graph/badge.svg
   :alt: Codecov badge
   :target: https://codecov.io/gh/carlosabalde/libvmod-gossip

VMOD useful to dump customizable information about contents currently stored in the cache to a JSON file for subsequent analysis in order to consider better caching strategies.

Looking for official support for this VMOD? Please, contact `Allenta Consulting <https://www.allenta.com>`_, a `Varnish Software Premium partner <https://www.varnish-software.com/partner/allenta-consulting>`_.

SYNOPSIS
========

import gossip;

::

    Function VOID dump(STRING file, BOOL discard=1)

    Function VOID discard()

    Function STRING escape_json_string(STRING value)

EXAMPLE
=======

/etc/varnish/default.vcl
------------------------

::

    vcl 4.0;

    import gossip;
    import std;

    backend default {
        .host = "127.0.0.1";
        .port = "8080";
    }

    acl internal {
        "localhost";
    }

    sub vcl_recv {
        if (req.url ~ "^/gossip/(?:dump/.+|discard/)$") {
            if (client.ip ~ internal) {
                if (req.url ~ "^/gossip/dump/.+") {
                    gossip.dump(regsub(req.url, "/gossip/dump", ""));
                    return (synth(200, "Now dumping."));
                } elsif (req.url == "/gossip/discard/") {
                    gossip.discard();
                    return (synth(200, "Discarded."));
                } else {
                    return (synth(404, "Not found."));
                }
            } else {
                return (synth(405, "Not allowed."));
            }
        }
    }

    sub vcl_deliver {
        unset resp.http.X-Gossip-Info;
    }

    sub vcl_backend_response {
        set beresp.http.X-Gossip-Info =
            {"{"} +
            {""url":""} + gossip.escape_json_string(bereq.http.Host + bereq.url) + {"","} +
            {""device":""} + gossip.escape_json_string(bereq.http.X-Device) + {"","} +
            {""ip":""} + gossip.escape_json_string(client.ip) + {"""} +
            {"}"};
        set beresp.ttl = 1h;
        set beresp.grace = 24h;
    }

Dump cache contents
-------------------

::

    $ curl http://127.0.0.1/gossip/dump/tmp/objects.json

    $ tail -f /tmp/objects.json
    {"tst"=0.000000,"now"=1526211998.300023}
    ...
    {"info": {"url":"127.0.0.1/foo","device":"desktop","ip":"127.0.0.1"},"hits":2,"ttl":-0.537690,"grace":10.000000,"keep":0.000000}
    {"info": {"url":"127.0.0.1/bar","device":"desktop","ip":"127.0.0.1"},"hits":0,"ttl":-0.524559,"grace":10.000000,"keep":0.000000}
    {"info": {"url":"127.0.0.1/bar","device":"mobile","ip":"127.0.0.1"},"hits":0,"ttl":-0.513125,"grace":10.000000,"keep":0.000000}

INSTALLATION
============

The source tree is based on autotools to configure the building, and does also have the necessary bits in place to do functional unit tests using the varnishtest tool.

**Beware this project contains multiples branches (master, 4.1, etc.). Please, select the branch to be used depending on your Varnish Cache version (Varnish trunk → master, Varnish 4.1.x → 4.1, etc.).**

COPYRIGHT
=========

See LICENSE for details.

BSD's implementation of the red–black tree and the splay tree data structures by Niels Provos has been borrowed from the `Varnish Cache project <https://github.com/varnishcache/varnish-cache>`_:

* https://github.com/varnishcache/varnish-cache/blob/master/include/vtree.h

Copyright (c) 2018 Carlos Abalde <carlos.abalde@gmail.com>
