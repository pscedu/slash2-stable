<?xml version="1.0" ?>
<!-- $Id: oof.xsl 21540 2013-06-01 05:41:22Z yanovich $ -->

<xsl:stylesheet
	version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:oof="http://www.psc.edu/~yanovich/xsl/oof-1.0"
	xmlns:utl="http://www.psc.edu/~yanovich/xsl/utl-1.0"
	extension-element-prefixes="utl">

<xsl:import href="utl.xsl" />
<xsl:output method="text" />

<xsl:variable name="nl">
	<xsl:text>
	</xsl:text>
</xsl:variable>

<xsl:variable name="tab">
	<xsl:text>	</xsl:text>
</xsl:variable>

<xsl:template name="oof_nestelem" match="oof:p|oof:div|oof:pre|oof:br|oof:hr|oof:span|oof:label|oof:table">
	<xsl:value-of select="$nl" />
	<xsl:text>$oof-&gt;</xsl:text>
	<xsl:value-of select="local-name()" />
	<xsl:text>(</xsl:text>
	<xsl:if test="count(@*) or local-name() = 'table'">
		<xsl:text>{</xsl:text>
		<xsl:apply-templates select="@*" />
		<xsl:text>},</xsl:text>
	</xsl:if>
	<xsl:choose>
		<xsl:when test="name() = 'oof:pre'">
			<xsl:apply-templates mode="preformatted" />
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates>
				<xsl:with-param name="nested" select="1" />
			</xsl:apply-templates>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:text>),</xsl:text>
</xsl:template>

<xsl:template mode="preformatted" match="oof:p|oof:div|oof:pre|oof:br|oof:hr|oof:span|oof:label|oof:table">
	<xsl:call-template name="oof_nestelem" />
</xsl:template>

<xsl:template match="oof:code|oof:emph|oof:strong|oof:email|oof:tt">
	<xsl:text>$oof-&gt;</xsl:text>
	<xsl:value-of select="local-name()" />
	<xsl:text>(</xsl:text>
	<xsl:apply-templates />
	<xsl:text>),</xsl:text>
</xsl:template>

<xsl:template match="oof:link">
	<xsl:text>$oof-&gt;link(</xsl:text>
	<xsl:text>value=&gt;[</xsl:text>
	<xsl:apply-templates />
	<xsl:text>], </xsl:text>
	<!--
		Any attributes that would be invalid in the generated Perl would
		probabably never pass through the XML, so we don't have to wrap
		them in quotes.
	-->
	<xsl:apply-templates select="@*" />
	<xsl:text>),</xsl:text>
</xsl:template>

<xsl:template match="@*">
	<xsl:value-of select="name()" />
	<xsl:text>=&gt;</xsl:text>
	<xsl:text>'</xsl:text>
	<xsl:call-template name="utl:escape-ents">
		<xsl:with-param name="string">
			<xsl:call-template name="utl:escape-code">
				<xsl:with-param name="string" select="current()" />
			</xsl:call-template>
		</xsl:with-param>
	</xsl:call-template>
	<xsl:text>',</xsl:text>
</xsl:template>

<!--
	Regular text; normalizes space between XML tags and escapes text and
	wraps around quotes.
-->
<xsl:template match="text()">
		<xsl:call-template name="utl:replace-string">
			<xsl:with-param name="string">
				<xsl:call-template name="stringify">
					<xsl:with-param name="str" select="current()" />
					<xsl:with-param name="pos" select="position()" />
					<xsl:with-param name="lastpos" select="last()" />
				</xsl:call-template>
			</xsl:with-param>
			<xsl:with-param name="search" select="'&amp;amp;escape-shit;'" />
			<xsl:with-param name="replace" select="'&amp;'" />
	</xsl:call-template>
</xsl:template>

<xsl:template name="stringify">
	<xsl:param name="str" />
	<xsl:param name="pos" />
	<xsl:param name="lastpos" />

	<xsl:if test="normalize-space($str)">
		<xsl:text>'</xsl:text>
		<xsl:if test="$pos != 1 and not(
		 starts-with(normalize-space($str), '.') or
		 starts-with(normalize-space($str), ',') or
		 starts-with(normalize-space($str), ';')) and
		 (starts-with($str, ' ') or
		  starts-with($str, $nl) or
		  starts-with($str, $tab)) ">
			<!-- XXX only add this space if space exists -->
			<xsl:text> </xsl:text>
		</xsl:if>
		<xsl:call-template name="utl:replace-string">
			<xsl:with-param name="string">
				<xsl:call-template name="utl:replace-string">
					<xsl:with-param name="string">
						<xsl:call-template name="utl:replace-string">
							<xsl:with-param name="string">
								<xsl:call-template name="utl:escape-ents">
									<xsl:with-param name="string">
									<xsl:call-template name="utl:escape-code">
										<xsl:with-param name="string" select="normalize-space($str)" />
									</xsl:call-template>
									</xsl:with-param>
								</xsl:call-template>
							</xsl:with-param>
							<xsl:with-param name="search" select="'``'" />
							<xsl:with-param name="replace" select="'&amp;#8220;'" />
						</xsl:call-template>
					</xsl:with-param>
					<!-- This should be escaped -->
					<xsl:with-param name="search" select="&quot;\'\'&quot;" />
					<xsl:with-param name="replace" select="'&amp;#8221;'" />
				</xsl:call-template>
			</xsl:with-param>
			<!-- This should be escaped -->
			<xsl:with-param name="search" select="&quot;\'&quot;" />
			<xsl:with-param name="replace" select="'&amp;#8217;'" />
		</xsl:call-template>
		<xsl:if test='$pos != $lastpos and (
		 substring($str, string-length($str)) = " " or
		 substring($str, string-length($str)) = $nl or
		 substring($str, string-length($str)) = $tab)'>
			<!-- XXX only add this space if space exists -->
			<xsl:text> </xsl:text>
		</xsl:if>
		<xsl:text>', </xsl:text>
	</xsl:if>
</xsl:template>

<!--
	Preformatted text; don't adjust whitespace
-->
<xsl:template match="text()" mode="preformatted">
	<xsl:if test="normalize-space(current())">
		<xsl:text>'</xsl:text>
		<xsl:call-template name="utl:escape-ents">
			<xsl:with-param name="string">
				<xsl:call-template name="utl:escape-code">
					<xsl:with-param name="string" select="current()" />
				</xsl:call-template>
			</xsl:with-param>
		</xsl:call-template>
		<xsl:text>', </xsl:text>
	</xsl:if>
</xsl:template>

<xsl:template match="oof:form">
	<xsl:text>
		$oof-&gt;form(
		</xsl:text>
	<xsl:if test="count(@*)">
		<xsl:text>{</xsl:text>
		<xsl:apply-templates select="@*" />
		<xsl:text>}</xsl:text>
		<xsl:if test="position() != last()">,</xsl:if>
	</xsl:if>
	<xsl:apply-templates />
	<xsl:text>)</xsl:text>
	<xsl:if test="position() != last()">, </xsl:if>
</xsl:template>

<xsl:template match="oof:table-row">
			<!-- XXX: if $_ is hash, need to put class inside -->
			<xsl:text>
				[</xsl:text>
			<xsl:apply-templates />
			<xsl:text>],</xsl:text>
</xsl:template>

<xsl:template match="oof:table-head|oof:table-cell">
	<xsl:text>{</xsl:text>
	<xsl:if test="name() = 'oof:table-head'">
		<xsl:text>table_head=&gt;1, </xsl:text>
	</xsl:if>
	<xsl:apply-templates select="@*" />
	<xsl:text>value=&gt;[</xsl:text>
	<xsl:apply-templates />
	<xsl:text>]</xsl:text>
	<xsl:text>}, </xsl:text>
</xsl:template>

<xsl:template match="oof:list-item">
	<xsl:text>
		$oof->list_item(</xsl:text>
	<xsl:apply-templates />
	<xsl:text>),</xsl:text>
</xsl:template>

<xsl:template match="oof:list">
	<xsl:text>
		$oof-&gt;list_start(OOF::</xsl:text>

	<xsl:choose>
		<xsl:when test="@type">
			<xsl:value-of select="@type" />
		</xsl:when>
		<xsl:otherwise>LIST_UN</xsl:otherwise>
	</xsl:choose>

	<xsl:if test="count(@*) > 1">
		<xsl:text>,</xsl:text>
		<xsl:apply-templates select="@*[not(name()='type')]" />
	</xsl:if>
	<xsl:text>),</xsl:text>

	<xsl:apply-templates>
		<xsl:with-param name="nested" select="1" />
	</xsl:apply-templates>

	<xsl:text>
		$oof-&gt;list_end(OOF::</xsl:text>

	<xsl:choose>
		<xsl:when test="@type">
			<xsl:value-of select="@type" />
		</xsl:when>
		<xsl:otherwise>LIST_UN</xsl:otherwise>
	</xsl:choose>

	<xsl:text>)</xsl:text>
	<xsl:if test="position() != last()">,
	</xsl:if>
</xsl:template>

<xsl:template match="oof:header|oof:map">
	<xsl:text>
		$oof-&gt;</xsl:text>
	<xsl:value-of select="local-name()" />
	<xsl:text>(</xsl:text>
	<xsl:if test="@*">
		<xsl:text>{</xsl:text>
		<!-- xsl:apply-templates select="@*" mode="preformatted" / -->
		<xsl:for-each select="@*">
			<xsl:value-of select="name()" />
			<xsl:text>=&gt;</xsl:text>
			<xsl:apply-templates mode="preformatted" />
			<xsl:if test="position() != last()">, </xsl:if>
		</xsl:for-each>
		<xsl:text>},</xsl:text>
	</xsl:if>
	<xsl:apply-templates />
	<xsl:text>)</xsl:text>
	<xsl:if test="position() != last()">, </xsl:if>
</xsl:template>

<xsl:template match="oof:img|oof:area|oof:canvas|oof:input">
	<xsl:text>
		$oof-&gt;</xsl:text>
	<xsl:value-of select="local-name()" />
	<xsl:text>(</xsl:text>
	<xsl:apply-templates select="@*" />
	<xsl:text>)</xsl:text>
	<xsl:if test="position() != last()">, </xsl:if>
</xsl:template>

<xsl:template match="processing-instruction()">
	<xsl:value-of select="current()" />
</xsl:template>

</xsl:stylesheet>
