<?xml version="1.0" ?>
<!-- $Id$ -->

<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:import href="oof2tex.xsl" />
<xsl:output method="text" />

<xsl:template match="xdc">
	<xsl:apply-templates />
</xsl:template>

<xsl:template match="title">
</xsl:template>

<xsl:template match="ref">
	<xsl:choose>
		<xsl:when test='@sect = "p"'>
			<xsl:text>$oof-&gt;link(href =&gt; "/devel/xdc.pwl?p=</xsl:text>
			<xsl:value-of select="current()" />
			<xsl:text>", value =&gt; join '', </xsl:text>
			<xsl:apply-templates />
			<xsl:text>),</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>$oof-&gt;link(href =&gt; "/mdoc.pwl?q=</xsl:text>
			<xsl:value-of select="current()" />
			<xsl:text>;sect=</xsl:text>
			<xsl:value-of select="@sect" />
			<xsl:text>", value =&gt; $oof-&gt;tt(join '', </xsl:text>
			<xsl:apply-templates />
			<xsl:text>'(</xsl:text>
			<xsl:value-of select="@sect" />
			<xsl:text>)')),</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

</xsl:stylesheet>
