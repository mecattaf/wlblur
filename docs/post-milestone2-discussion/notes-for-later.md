Must address the pros and cons of having a dedicated blur provider configuration vs having it done in-config.

Since it's exposed to a socket after all, we could just start it with `exec wlblur`and like kanshi add some flags to it like `-config` for nonstandard locations

And - since we can - make the daemon hot-reloadable so that we can dynamically experiment with different configurations.
