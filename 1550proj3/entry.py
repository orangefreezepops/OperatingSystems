class Entry:
    def __init__(self, framenum, pagenum, refbit, dirtybit, validbit):
        #page number of the entry
        self.__pagenum = pagenum
        #frame number of the entry
        self.__framenum = framenum
        #reference bit
        self.__refbit = refbit
        #dirty bit
        self.__dirtybit = dirtybit
        #valid bit
        self.__validbit = validbit

    #getters and setters
    def get_pagenum(self):
        return self.__pagenum

    def set_pagenum(self, pn):
        self.__pagenum = pn

    def get_framnum(self):
        return self.__framenum

    def set_framnum(self, fn):
        self.__framenum = fn

    def get_refbit(self):
        return self.__refbit

    def set_refbit(self, rb):
        self.__refbit = rb

    def get_dirtybit(self):
        return self.__dirtybit

    def set_dirtybit(self, db):
        self.__dirtybit = db

    def get_validbit(self):
        return self.__validbit

    def set_validbit(self, vb):
        self.__validbit = vb
