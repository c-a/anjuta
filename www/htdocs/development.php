<!--
	<h3>Anjuta hourly CVS tarball update</h3>
	<p>
       
		<?php
			$filename = "cvs/index.html";
			$handle = fopen($filename, "r");
			$contents = fread($handle, filesize($filename));
			$contents = preg_replace("/\n/m", "", $contents);
			$contents = preg_replace("/^.*<ul>/i", "<ul>", $contents);
			$contents = preg_replace("/<\/ul>.*$/i", "</ul>", $contents);
                        $contents .= "You can also browse the CVS online at <a href=\"http://cvs.gnome.org/viewcvs/anjuta/\">GNOME ViewCVS interface</a>";
			print($contents);

		?>
	</p>
-->
	<h3>Getting Anjuta from GNOME subversion server</h3>
	<p>
		Subversion is a powerful tool to allow many developers work on the
		same source code.  This is possible because each developer checks out a copy of
		the current version of the source code.  Then, each developer independently works on
		his or her own personal copy of the sources. When the developer has made
		changes, he or she commits them back to the subversion repository. 
		The subversion server then takes care of things like trying
		to merge each developer's changes with those of others.  When that doesn't work, the
		developer is notified, and he or she must do a hand merge of the conflicts.
		</p>
		<p>First, you need to set up the editor environmental variable, for example:
		</p>
		<div class="terminal">
			$ export EDITOR='emacs'
		</div>

		<p>Make sure you have all the dependencies of Anjuta installed and ready.
		Now, grab the anjuta, gnome-build and gdl modules from the repository:</p>
		
		<div class="terminal">
		$ svn checkout http://svn.gnome.org/svn/gdl/trunk gdl <br/>
		$ svn checkout http://svn.gnome.org/svn/gnome-build/trunk gnome-build <br/>
		$ svn checkout http://svn.gnome.org/svn/anjuta/trunk anjuta
		</div>

		<p>That only gives read access to the repository. Instead, if you have an account
		in svn.gnome.org and want write access (you must have been granted write access
		to anjuta module as an Anjuta developer), you need to checkout the source with:</p>
		
		<div class="terminal">
		$ svn checkout svn+ssh://svn.gnome.org/svn/gdl/trunk gdl <br/>
		$ svn checkout svn+ssh://svn.gnome.org/svn/gnome-build/trunk gnome-build <br/>
		$ svn checkout svn+ssh://svn.gnome.org/svn/anjuta/trunk anjuta
		</div>
		
		<p>Now, you will want to go through them to build and install.  Run:</p>

		<div class="terminal">$ ./autogen.sh</div>

		<p>If you want to enabled Anjuta API docs generation, you can pass --enable-gtk-doc to
                the above autogen.sh script. Then continue just like you would work with a tarball
                package:</p>
                <div class="terminal">
		$ ./configure<br/>
		$ make<br/>
		$ su [password]<br/>
		# make install<br/>
                </div>

		<p>After you have checked the code out, you can use:</p>
                <div class="terminal">
			$ svn update (in the anjuta	top level directory)
                </div>
		<p>to update your personal copy of anjuta to the latest version from the
		subversion repository and rebuild.
                </p>

        <h3>Prolog</h3>
	<p>&quot;A service to open source is a service to mankind.&quot;&nbsp;</p>
	<p>
		I have spent lots of time and energy on the development of Anjuta, and
		the only thing I expect in return is your generous help in its
		further development.  It took me quite some time to bring the first
		release of Anjuta to the world in 1999.  For quite some time, the development of
		Anjuta was just &quot;a one man show&quot;, but very soon it acquired
		quite a bit of attention, and now I am proud to say that Anjuta
		has a full-fledged development team!  I am very thankful to all the
		people who have contributed and continue contributing to the further
		development of Anjuta as &quot;a service to Mankind.&quot;&nbsp;
		<br/><br/>
		-Naba
	</p>

<h3>History</h3>
	<p><b>27th Dec 1997 ...</b></p>
	<p>
		I had a date with Anjuta (my GF and not
		the program).  It was really after a long time that I was going to meet
		her.  We usually do not meet often because of the 'geographical distance'.
		Coincidentally, that day was also her birthday.  I was
		feeling very happy and had decided to celebrate her birthday with a
		little piece of cake.  Well, because my luck was not good that day, I could not
		celebrate her birthday.  This made me feel as though I'd been hit by a billion
		bullets.  I could not even do such a little thing for her (after all, we were
		meeting after a long time).  Simply because I could not forget the incident, I
		decided to do something for her solely from my side.  And I did it.  Meantime,
		Gnome had been lacking a good build environment (by 'good' I mean user
		friendly).  And many people, especially my friends, those who swear by M$, had
		been commenting: &quot;Of course, Linux has become user-friendly.  See, you can
		now click buttons.  But the thing is, you don't feel like clicking them. &quot; I
		just wanted them to 'feel like clicking them.'
		<br/>
		<br/>
		Then, I started the journey ...
		</br/>
		</br/>
		-Naba
	</p>